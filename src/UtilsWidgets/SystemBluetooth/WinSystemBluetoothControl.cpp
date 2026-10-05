#include "WinSystemBluetoothControl.h"

#include <winsock2.h>
#include <windows.h>
#include <bluetoothapis.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>

#include <QDesktopServices>
#include <QUrl>
#include <QStringList>
#include <optional>
#include <shellapi.h>
#include <vector>
#include <string>

namespace {

    void ensureWinRtApartment() {
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
        }
        catch (...) {
            // Ya inicializado en este hilo
        }
    }

    std::optional<winrt::Windows::Devices::Radios::Radio> findBluetoothRadio() {
        using namespace winrt::Windows::Devices::Radios;
        try {
            ensureWinRtApartment();

            auto access = Radio::RequestAccessAsync().get();
            if (access != RadioAccessStatus::Allowed) {
                return std::nullopt;
            }

            auto radios = Radio::GetRadiosAsync().get();
            for (auto const& radio : radios) {
                if (radio.Kind() == RadioKind::Bluetooth)
                    return radio;
            }
        }
        catch (...) {
        }
        return std::nullopt;
    }

    uint64_t macToUint64(const QString& mac) {
        bool ok = false;
        QString cleanMac = mac;
        cleanMac.remove(':');
        return cleanMac.toULongLong(&ok, 16);
    }

    QString formatMac(const BLUETOOTH_ADDRESS& a) {
        return QString("%1:%2:%3:%4:%5:%6")
            .arg(a.rgBytes[5], 2, 16, QChar('0'))
            .arg(a.rgBytes[4], 2, 16, QChar('0'))
            .arg(a.rgBytes[3], 2, 16, QChar('0'))
            .arg(a.rgBytes[2], 2, 16, QChar('0'))
            .arg(a.rgBytes[1], 2, 16, QChar('0'))
            .arg(a.rgBytes[0], 2, 16, QChar('0'))
            .toUpper();
    }

    // --- FUNCIONES AUXILIARES WIN32 ---

    bool parseMac(const QString& mac, BLUETOOTH_ADDRESS& out) {
        const QStringList parts = mac.split(':');
        if (parts.size() != 6)
            return false;

        out.ullLong = 0;
        for (int i = 0; i < 6; ++i) {
            bool ok = false;
            const uint value = parts[i].toUInt(&ok, 16);
            if (!ok || value > 0xFF)
                return false;
            out.rgBytes[5 - i] = static_cast<BYTE>(value);
        }
        return true;
    }

    bool loadDeviceInfo(const QString& mac, BLUETOOTH_DEVICE_INFO& info) {
        ZeroMemory(&info, sizeof(info));
        info.dwSize = sizeof(info);
        if (!parseMac(mac, info.Address))
            return false;
        return BluetoothGetDeviceInfo(nullptr, &info) == ERROR_SUCCESS;
    }

    bool setAllServices(BLUETOOTH_DEVICE_INFO& info, DWORD flag) {
        GUID guids[32];
        DWORD count = 32;
        if (BluetoothEnumerateInstalledServices(nullptr, &info, &count, guids) != ERROR_SUCCESS)
            return false;

        bool anyOk = false;
        for (DWORD i = 0; i < count; ++i) {
            if (BluetoothSetServiceState(nullptr, &info, &guids[i], flag) == ERROR_SUCCESS)
                anyOk = true;
        }
        return anyOk;
    }

    GUID bluetoothGuid(unsigned long shortId) {
        return GUID{ shortId, 0x0000, 0x1000,
                     { 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB } };
    }

    // Perfiles habituales: A2DP, AVRCP, Headset, Hands-Free, HID
    const unsigned long kKnownServiceIds[] = {
        0x1108, 0x110A, 0x110B, 0x110C, 0x110D, 0x110E,
        0x1112, 0x111E, 0x111F, 0x1124
    };


    // Estado de conexión según WinRT. nullopt si no se pudo determinar.
    std::optional<bool> winrtIsConnected(uint64_t address) {
        using namespace winrt::Windows::Devices::Bluetooth;
        try {
            ensureWinRtApartment();
            auto device = BluetoothDevice::FromBluetoothAddressAsync(address).get();
            if (!device)
                return std::nullopt;
            const bool connected =
                device.ConnectionStatus() == BluetoothConnectionStatus::Connected;
            device.Close();
            return connected;
        }
        catch (...) {
            return std::nullopt;
        }
    }

    std::vector<GUID> collectServiceGuids(BLUETOOTH_DEVICE_INFO& info) {
        std::vector<GUID> guids;

        GUID enumerated[32];
        DWORD count = 32;
        if (BluetoothEnumerateInstalledServices(nullptr, &info, &count, enumerated) == ERROR_SUCCESS)
            guids.assign(enumerated, enumerated + count);

        for (unsigned long id : kKnownServiceIds) {
            const GUID g = bluetoothGuid(id);
            bool exists = false;
            for (const GUID& e : guids) {
                if (IsEqualGUID(e, g)) { exists = true; break; }
            }
            if (!exists)
                guids.push_back(g);
        }
        return guids;
    }

    bool applyServiceState(BLUETOOTH_DEVICE_INFO& info, DWORD flag) {
        bool anyOk = false;
        std::vector<GUID> guids = collectServiceGuids(info);
        for (GUID& g : guids) {
            // Los GUID que el dispositivo no soporta devuelven error: se ignoran
            if (BluetoothSetServiceState(nullptr, &info, &g, flag) == ERROR_SUCCESS)
                anyOk = true;
        }
        return anyOk;
    }

    bool enableAllServices(BLUETOOTH_DEVICE_INFO& info) { return applyServiceState(info, BLUETOOTH_SERVICE_ENABLE); }
    bool disableAllServices(BLUETOOTH_DEVICE_INFO& info) { return applyServiceState(info, BLUETOOTH_SERVICE_DISABLE); }

} // namespace

bool WinSystemBluetoothControl::isPowered() {
    ensureWinRtApartment();
    try {
        const auto radio = findBluetoothRadio();
        if (!radio)
            return false;

        return radio->State() == winrt::Windows::Devices::Radios::RadioState::On;
    }
    catch (...) {
        return false;
    }
}

bool WinSystemBluetoothControl::setPowered(bool enabled) {
    ensureWinRtApartment();
    try {
        const auto radio = findBluetoothRadio();
        if (!radio)
            return false;

        using namespace winrt::Windows::Devices::Radios;
        const auto status = radio->SetStateAsync(enabled ? RadioState::On : RadioState::Off).get();
        return status == RadioAccessStatus::Allowed;
    }
    catch (...) {
        return false;
    }
}

QList<BluetoothDeviceInfo> WinSystemBluetoothControl::knownDevices() {
    QList<BluetoothDeviceInfo> devices;

    BLUETOOTH_DEVICE_SEARCH_PARAMS params;
    ZeroMemory(&params, sizeof(params));
    params.dwSize = sizeof(params);
    params.fReturnAuthenticated = TRUE;
    params.fReturnRemembered = TRUE;
    params.fReturnConnected = TRUE;
    params.fReturnUnknown = FALSE;
    params.fIssueInquiry = FALSE;
    params.cTimeoutMultiplier = 0;
    params.hRadio = nullptr;

    BLUETOOTH_DEVICE_INFO info;
    ZeroMemory(&info, sizeof(info));
    info.dwSize = sizeof(info);

    HBLUETOOTH_DEVICE_FIND find = BluetoothFindFirstDevice(&params, &info);
    if (!find)
        return devices;

    do {
        BluetoothDeviceInfo dev;
        dev.mac = formatMac(info.Address);
        dev.name = QString::fromWCharArray(info.szName);

        bool connected = info.fConnected;
        if (connected) {
            // fConnected puede quedarse desactualizado tras una desconexión: contrastar con WinRT
            const auto real = winrtIsConnected(info.Address.ullLong);
            if (real.has_value() && !*real)
                connected = false;
        }
        dev.connected = connected;

        devices.append(dev);
    } while (BluetoothFindNextDevice(find, &info));

    BluetoothFindDeviceClose(find);
    return devices;
}

bool WinSystemBluetoothControl::connectDevice(const QString& mac) {
    ensureWinRtApartment();
    const uint64_t address = macToUint64(mac);
    if (address == 0)
        return false;

    using namespace winrt::Windows::Devices::Bluetooth;
    using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

    // 0. Forzar la reconexión de los servicios.
    //    Si Windows ya los tenía habilitados (p. ej. conectaste y desconectaste desde
    //    Configuración), un ENABLE directo no hace nada: hay que ciclar DISABLE -> ENABLE.
    bool servicesEnabled = false;
    {
        BLUETOOTH_DEVICE_INFO info;
        if (loadDeviceInfo(mac, info)) {
            if (!info.fConnected) {
                setAllServices(info, BLUETOOTH_SERVICE_DISABLE);
                Sleep(300);   // estamos en un hilo de trabajo, no bloquea la UI
            }
            servicesEnabled = enableAllServices(info);
        }
    }

    // 1. Probar Bluetooth Classic con RAII
    try {
        auto device = BluetoothDevice::FromBluetoothAddressAsync(address).get();
        if (device) {
            struct ClassicCleaner {
                BluetoothDevice& dev;
                ~ClassicCleaner() { dev.Close(); }
            } cleaner{ device };

            if (device.ConnectionStatus() == BluetoothConnectionStatus::Connected) {
                return true;
            }

            auto rfcommResult = device.GetRfcommServicesAsync(BluetoothCacheMode::Cached).get();
            if (device.ConnectionStatus() == BluetoothConnectionStatus::Connected) {
                return true;
            }
        }
    }
    catch (...) {
    }

    // 2. Probar Bluetooth Low Energy (BLE) con RAII
    try {
        auto bleDevice = BluetoothLEDevice::FromBluetoothAddressAsync(address).get();
        if (bleDevice) {
            struct BleCleaner {
                BluetoothLEDevice& dev;
                ~BleCleaner() { dev.Close(); }
            } cleaner{ bleDevice };

            if (bleDevice.ConnectionStatus() == BluetoothConnectionStatus::Connected) {
                return true;
            }

            auto gattResult = bleDevice.GetGattServicesAsync(BluetoothCacheMode::Uncached).get();
            if (gattResult.Status() == GattCommunicationStatus::Success ||
                bleDevice.ConnectionStatus() == BluetoothConnectionStatus::Connected) {
                return true;
            }
        }
    }
    catch (...) {
    }

    // 3. Resultado del habilitado Win32. true = el SO aceptó la petición;
    //    la conexión real la verifica la ventana con knownDevices().
    return servicesEnabled;
}

bool WinSystemBluetoothControl::disconnectDevice(const QString& mac) {
    ensureWinRtApartment();

    bool disabledAny = false;
    bool disconnected = false;

    for (int attempt = 0; attempt < 3; ++attempt) {
        BLUETOOTH_DEVICE_INFO info;
        if (!loadDeviceInfo(mac, info))
            break;

        bool connectedNow = info.fConnected;

        if (connectedNow) {
            const auto real = winrtIsConnected(info.Address.ullLong);
            if (real.has_value() && !*real)
                connectedNow = false;
        }

        if (!connectedNow) {
            disconnected = true;
            break;
        }

        if (disableAllServices(info))
            disabledAny = true;

        Sleep(700);   // hilo de trabajo: no bloquea la UI
    }

    return disconnected || disabledAny;
}

bool WinSystemBluetoothControl::openBluetoothSettings() {
    return QDesktopServices::openUrl(QUrl("ms-settings:bluetooth"));
}

bool WinSystemBluetoothControl::restartAdapter() {
    ensureWinRtApartment();   // ShellExecuteEx requiere COM inicializado en este hilo

    // Ruta absoluta a cmd.exe para evitar que se resuelva un ejecutable falso por PATH
    wchar_t sysDir[MAX_PATH];
    if (GetSystemDirectoryW(sysDir, MAX_PATH) == 0)
        return false;
    const std::wstring cmdPath = std::wstring(sysDir) + L"\\cmd.exe";

    // Comando fijo. "/y" confirma automáticamente la parada de servicios dependientes
    // (si no, net stop se quedaría esperando un Y/N que nadie puede contestar).
    // Se usa "&" y no "&&" para que net start se ejecute aunque el servicio ya estuviera detenido.
    const wchar_t* params = L"/c net stop bthserv /y & net start bthserv";

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";                 // pide elevación (UAC)
    sei.lpFile = cmdPath.c_str();
    sei.lpParameters = params;
    sei.nShow = SW_HIDE;

    if (!ShellExecuteExW(&sei))
        return false;                      // p. ej. el usuario canceló el UAC (ERROR_CANCELLED)

    if (!sei.hProcess)
        return false;

    const DWORD wait = WaitForSingleObject(sei.hProcess, 30000);
    DWORD exitCode = 1;
    if (wait == WAIT_OBJECT_0)
        GetExitCodeProcess(sei.hProcess, &exitCode);
    CloseHandle(sei.hProcess);

    return wait == WAIT_OBJECT_0 && exitCode == 0;
}