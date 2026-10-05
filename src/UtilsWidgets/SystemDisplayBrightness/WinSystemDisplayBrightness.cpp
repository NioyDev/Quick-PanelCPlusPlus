#include "WinSystemDisplayBrightness.h"


#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <wbemidl.h>
#include <highlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>

#include <algorithm>
#include <vector>

namespace {

    // ============================================================================
    // Utilidades RAII (sin depender de WRL/ATL, compatible con MSVC y MinGW)
    // ============================================================================
    template <typename T>
    class ComPtr {
    public:
        ComPtr() = default;
        ~ComPtr() { reset(); }
        ComPtr(const ComPtr&) = delete;
        ComPtr& operator=(const ComPtr&) = delete;

        T** put() { reset(); return &m_ptr; }
        void** putVoid() { reset(); return reinterpret_cast<void**>(&m_ptr); }
        T* get() const { return m_ptr; }
        T* operator->() const { return m_ptr; }
        explicit operator bool() const { return m_ptr != nullptr; }
        void reset() { if (m_ptr) { m_ptr->Release(); m_ptr = nullptr; } }

    private:
        T* m_ptr = nullptr;
    };

    class Bstr {
    public:
        explicit Bstr(const wchar_t* s) : m_str(SysAllocString(s)) {}
        ~Bstr() { SysFreeString(m_str); }
        Bstr(const Bstr&) = delete;
        Bstr& operator=(const Bstr&) = delete;
        BSTR get() const { return m_str; }

    private:
        BSTR m_str;
    };

    // Qt ya inicializa COM en el hilo principal; aquí solo nos aseguramos de que
    // el hilo actual tenga COM y balanceamos el Uninitialize cuando corresponde.
    class ComScope {
    public:
        ComScope()
        {
            const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            m_needsUninit = SUCCEEDED(hr);                 // S_OK o S_FALSE
            m_ok = m_needsUninit || hr == RPC_E_CHANGED_MODE; // ya iniciado con otro modelo: sirve igual
        }
        ~ComScope() { if (m_needsUninit) CoUninitialize(); }
        ComScope(const ComScope&) = delete;
        ComScope& operator=(const ComScope&) = delete;
        bool ok() const { return m_ok; }

    private:
        bool m_needsUninit = false;
        bool m_ok = false;
    };

    // ============================================================================
    // Panel interno: WMI (ROOT\WMI)
    // ============================================================================
    void setImpersonation(IUnknown* proxy)
    {
        CoSetProxyBlanket(proxy, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
            nullptr, EOAC_NONE);
    }

    bool connectWmi(ComPtr<IWbemServices>& services)
    {
        ComPtr<IWbemLocator> locator;
        if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, locator.putVoid()))) {
            return false;
        }

        Bstr ns(L"ROOT\\WMI");
        if (FAILED(locator->ConnectServer(ns.get(), nullptr, nullptr, nullptr, 0,
            nullptr, nullptr, services.put()))) {
            return false;
        }

        setImpersonation(services.get());
        return true;
    }

    // Recorre los objetos de un query WQL. `fn` devuelve false para detener.
    template <typename Fn>
    void forEachWmiObject(IWbemServices* services, const wchar_t* wql, Fn&& fn)
    {
        Bstr language(L"WQL");
        Bstr query(wql);

        ComPtr<IEnumWbemClassObject> enumerator;
        if (FAILED(services->ExecQuery(language.get(), query.get(),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr, enumerator.put()))) {
            return;
        }
        setImpersonation(enumerator.get());

        constexpr LONG kTimeoutMs = 5000;
        for (;;) {
            ComPtr<IWbemClassObject> object;
            ULONG returned = 0;
            const HRESULT hr = enumerator->Next(kTimeoutMs, 1, object.put(), &returned);
            if (FAILED(hr) || returned == 0)
                break;
            if (!fn(object.get()))
                break;
        }
    }

    std::optional<int> wmiRead()
    {
        ComScope com;
        if (!com.ok())
            return std::nullopt;

        ComPtr<IWbemServices> services;
        if (!connectWmi(services))
            return std::nullopt;

        std::optional<int> result;
        forEachWmiObject(
            services.get(),
            L"SELECT CurrentBrightness FROM WmiMonitorBrightness WHERE Active=TRUE",
            [&result](IWbemClassObject* object) {
                VARIANT value;
                VariantInit(&value);
                if (SUCCEEDED(object->Get(L"CurrentBrightness", 0, &value, nullptr, nullptr)) &&
                    SUCCEEDED(VariantChangeType(&value, &value, 0, VT_I4))) {
                    result = static_cast<int>(value.lVal);
                }
                VariantClear(&value);
                return !result.has_value(); // seguir solo si aún no hay valor
            });
        return result;
    }

    bool wmiWrite(int percent)
    {
        ComScope com;
        if (!com.ok())
            return false;

        ComPtr<IWbemServices> services;
        if (!connectWmi(services))
            return false;

        // Firma del método WmiSetBrightness(Timeout, Brightness)
        Bstr className(L"WmiMonitorBrightnessMethods");
        ComPtr<IWbemClassObject> classObject;
        if (FAILED(services->GetObject(className.get(), 0, nullptr, classObject.put(), nullptr)))
            return false;

        ComPtr<IWbemClassObject> inSignature;
        if (FAILED(classObject->GetMethod(L"WmiSetBrightness", 0, inSignature.put(), nullptr)) ||
            !inSignature) {
            return false;
        }

        Bstr methodName(L"WmiSetBrightness");
        bool applied = false;

        forEachWmiObject(
            services.get(),
            L"SELECT * FROM WmiMonitorBrightnessMethods WHERE Active=TRUE",
            [&](IWbemClassObject* instance) {
                VARIANT path;
                VariantInit(&path);
                if (FAILED(instance->Get(L"__PATH", 0, &path, nullptr, nullptr)) ||
                    path.vt != VT_BSTR) {
                    VariantClear(&path);
                    return true;
                }

                ComPtr<IWbemClassObject> inParams;
                if (SUCCEEDED(inSignature->SpawnInstance(0, inParams.put()))) {
                    VARIANT timeout;
                    VariantInit(&timeout);
                    timeout.vt = VT_I4;
                    timeout.lVal = 0; // aplicar de inmediato

                    VARIANT brightness;
                    VariantInit(&brightness);
                    brightness.vt = VT_UI1;
                    brightness.bVal = static_cast<BYTE>(percent);

                    if (SUCCEEDED(inParams->Put(L"Timeout", 0, &timeout, 0)) &&
                        SUCCEEDED(inParams->Put(L"Brightness", 0, &brightness, 0))) {
                        const HRESULT hr = services->ExecMethod(path.bstrVal, methodName.get(), 0,
                            nullptr, inParams.get(),
                            nullptr, nullptr);
                        applied = SUCCEEDED(hr) || applied;
                    }
                }

                VariantClear(&path);
                return true; // continuar: puede haber más de un panel interno
            });

        return applied;
    }

    // ============================================================================
    // Monitores externos: DDC/CI (Monitor Configuration API)
    // ============================================================================
    BOOL CALLBACK collectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM param)
    {
        reinterpret_cast<std::vector<HMONITOR>*>(param)->push_back(monitor);
        return TRUE;
    }

    // Recorre todos los monitores físicos de todas las pantallas.
    // `fn(HANDLE)` devuelve false para detener.
    template <typename Fn>
    void forEachPhysicalMonitor(Fn&& fn)
    {
        std::vector<HMONITOR> monitors;
        EnumDisplayMonitors(nullptr, nullptr, collectMonitor,
            reinterpret_cast<LPARAM>(&monitors));

        for (HMONITOR monitor : monitors) {
            DWORD count = 0;
            if (!GetNumberOfPhysicalMonitorsFromHMONITOR(monitor, &count) || count == 0)
                continue;

            std::vector<PHYSICAL_MONITOR> physical(count);
            if (!GetPhysicalMonitorsFromHMONITOR(monitor, count, physical.data()))
                continue;

            bool keepGoing = true;
            for (auto& pm : physical) {
                if (keepGoing)
                    keepGoing = fn(pm.hPhysicalMonitor);
            }
            DestroyPhysicalMonitors(count, physical.data());

            if (!keepGoing)
                break;
        }
    }

    std::optional<int> ddcRead()
    {
        std::optional<int> result;
        forEachPhysicalMonitor([&result](HANDLE monitor) {
            DWORD minB = 0, curB = 0, maxB = 0;
            if (GetMonitorBrightness(monitor, &minB, &curB, &maxB) && maxB > minB)
                result = static_cast<int>((curB - minB) * 100 / (maxB - minB));
            return !result.has_value();
            });
        return result;
    }

    bool ddcWrite(int percent)
    {
        bool applied = false;
        forEachPhysicalMonitor([&](HANDLE monitor) {
            DWORD minB = 0, curB = 0, maxB = 0;
            // Cada monitor reporta su propio rango (100, 255, etc.)
            if (GetMonitorBrightness(monitor, &minB, &curB, &maxB) && maxB > minB) {
                const DWORD value = minB + static_cast<DWORD>(percent) * (maxB - minB) / 100;
                applied = SetMonitorBrightness(monitor, value) || applied;
            }
            return true; // aplicar a todos los monitores compatibles
            });
        return applied;
    }

} // namespace

bool WinSystemDisplayBrightness::isAvailable() const
{
    return brightnessPercent().has_value();
}

std::optional<int> WinSystemDisplayBrightness::brightnessPercent() const
{
    std::optional<int> percent = wmiRead();
    if (!percent)
        percent = ddcRead();

    if (!percent)
        return std::nullopt;
    return std::clamp(*percent, 0, 100);
}

bool WinSystemDisplayBrightness::setBrightnessPercent(int percent)
{
    percent = std::clamp(percent, 0, 100);

    // Sin cortocircuito: si hay laptop + monitor externo, se actualizan ambos.
    const bool internalOk = wmiWrite(percent);
    const bool externalOk = ddcWrite(percent);
    return internalOk || externalOk;
}
