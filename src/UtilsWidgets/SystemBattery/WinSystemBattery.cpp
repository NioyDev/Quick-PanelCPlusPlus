#include "WinSystemBattery.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cmath>
#include <vector>

#include <windows.h>
#include <winioctl.h>
#include <poclass.h>
#include <setupapi.h>
#include <batclass.h>

// WINAPI ES UNA MIERDA
#pragma comment(lib, "setupapi.lib")

namespace {

    const GUID kGuidDeviceBattery = {
        0x72631e54, 0x78a4, 0x11d0,
        { 0xbc, 0xf7, 0x00, 0xaa, 0x00, 0xb7, 0xb3, 0x2a }
    };

    // GUIDs de overlay schemes (Windows 10/11). Pueden variar entre versiones.
    const GUID kOverlayPowerSaver = { 0x961cc777, 0x2547, 0x4f9d, {0x81, 0x74, 0x7d, 0x86, 0x18, 0x1b, 0x8a, 0x7a} };
    const GUID kOverlayBalanced = { 0x00000000, 0x0000, 0x0000, {0, 0, 0, 0, 0, 0, 0, 0} };
    const GUID kOverlayBetterPerf = { 0x3af9b8d9, 0x7c97, 0x431d, {0xad, 0x78, 0x34, 0xa8, 0xbf, 0xea, 0x43, 0x9f} };
    const GUID kOverlayPerformance = { 0xded574b5, 0x45a0, 0x4f42, {0x87, 0x37, 0x46, 0x34, 0x5c, 0x09, 0xc2, 0x38} };

    GUID overlayFor(PowerProfile p)
    {
        switch (p) {
        case PowerProfile::PowerSaver:  return kOverlayPowerSaver;
        case PowerProfile::Balanced:    return kOverlayBalanced;
        case PowerProfile::Performance: return kOverlayPerformance;
        }
        return kOverlayBalanced;
    }

    struct BatteryHandle {
        HANDLE handle = INVALID_HANDLE_VALUE;
        ULONG tag = 0;
        BatteryHandle() = default;
        BatteryHandle(const BatteryHandle&) = delete;
        BatteryHandle& operator=(const BatteryHandle&) = delete;
        ~BatteryHandle()
        {
            if (handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);
        }
    };

    bool openFirstBattery(BatteryHandle& out)
    {
        // Cambiar &GUID_DEVICE_BATTERY por &kGuidDeviceBattery
        HDEVINFO devs = SetupDiGetClassDevsW(&kGuidDeviceBattery, nullptr, nullptr,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (devs == INVALID_HANDLE_VALUE)
            return false;

        bool found = false;
        SP_DEVICE_INTERFACE_DATA did{};
        did.cbSize = sizeof(did);

        for (DWORD i = 0; !found && SetupDiEnumDeviceInterfaces(devs, nullptr, &kGuidDeviceBattery, i, &did); ++i) {
            DWORD size = 0;
            SetupDiGetDeviceInterfaceDetailW(devs, &did, nullptr, 0, &size, nullptr);
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                continue;

            std::vector<char> buffer(size);
            auto* detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            if (!SetupDiGetDeviceInterfaceDetailW(devs, &did, detail, size, &size, nullptr))
                continue;

            HANDLE h = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h == INVALID_HANDLE_VALUE)
                continue;

            ULONG wait = 0, tag = 0;
            DWORD returned = 0;
            if (DeviceIoControl(h, IOCTL_BATTERY_QUERY_TAG, &wait, sizeof(wait), &tag, sizeof(tag),
                &returned, nullptr) && tag != 0) {
                out.handle = h;
                out.tag = tag;
                found = true;
            }
            else {
                CloseHandle(h);
            }
        }
        SetupDiDestroyDeviceInfoList(devs);
        return found;
    }

    template <typename T>
    bool queryInformation(const BatteryHandle& b, BATTERY_QUERY_INFORMATION_LEVEL level, T& out)
    {
        BATTERY_QUERY_INFORMATION q{};
        q.BatteryTag = b.tag;
        q.InformationLevel = level;
        DWORD returned = 0;
        return DeviceIoControl(b.handle, IOCTL_BATTERY_QUERY_INFORMATION, &q, sizeof(q),
            &out, sizeof(out), &returned, nullptr) != 0;
    }

} // namespace

WinSystemBattery::WinSystemBattery()
    : m_powrprof(QStringLiteral("powrprof"))
{
    if (m_powrprof.load()) {
        m_getOverlay = reinterpret_cast<GetOverlayFn>(m_powrprof.resolve("PowerGetEffectiveOverlayScheme"));
        m_setOverlay = reinterpret_cast<SetOverlayFn>(m_powrprof.resolve("PowerSetActiveOverlayScheme"));
    }
}

BatteryInfo WinSystemBattery::readBattery()
{
    BatteryInfo info;

    SYSTEM_POWER_STATUS sps{};
    if (!GetSystemPowerStatus(&sps) || (sps.BatteryFlag & 128) || sps.BatteryLifePercent == 255)
        return info;

    info.present = true;
    info.capacityPercent = sps.BatteryLifePercent;

    const bool onAc = sps.ACLineStatus == 1;
    const bool charging = (sps.BatteryFlag & 8) != 0;
    if (charging)
        info.state = BatteryState::Charging;
    else if (!onAc)
        info.state = BatteryState::Discharging;
    else
        info.state = info.capacityPercent >= 100 ? BatteryState::Full : BatteryState::NotCharging;

    BatteryHandle battery;
    if (!openFirstBattery(battery))
        return info;

    // Capacidad de diseño vs. capacidad completa actual (mWh)
    BATTERY_INFORMATION bi{};
    if (queryInformation(battery, BatteryInformation, bi) && bi.DesignedCapacity > 0)
        info.healthPercent = (double(bi.FullChargedCapacity) / double(bi.DesignedCapacity)) * 100.0;

    // Temperatura en décimas de Kelvin (muchos equipos no la exponen)
    ULONG tenthsKelvin = 0;
    if (queryInformation(battery, BatteryTemperature, tenthsKelvin) && tenthsKelvin > 0)
        info.temperatureCelsius = (double(tenthsKelvin) / 10.0) - 273.15;

    // Consumo y tiempo restante
    BATTERY_WAIT_STATUS wait{};
    wait.BatteryTag = battery.tag;
    BATTERY_STATUS status{};
    DWORD returned = 0;
    if (DeviceIoControl(battery.handle, IOCTL_BATTERY_QUERY_STATUS, &wait, sizeof(wait),
        &status, sizeof(status), &returned, nullptr)
        && status.Rate != BATTERY_UNKNOWN_RATE) {
        const double rateMw = std::abs(double(status.Rate));
        info.powerWatts = rateMw / 1000.0;

        if (rateMw > 0.0 && status.Capacity != BATTERY_UNKNOWN_CAPACITY) {
            std::optional<double> remaining;
            if (info.state == BatteryState::Charging && bi.FullChargedCapacity > 0)
                remaining = std::max(0.0, double(bi.FullChargedCapacity) - double(status.Capacity));
            else if (info.state == BatteryState::Discharging)
                remaining = double(status.Capacity);
            if (remaining)
                info.remainingMinutes = static_cast<int>((*remaining / rateMw) * 60.0);
        }
    }
    return info;
}

QList<PowerProfile> WinSystemBattery::availableProfiles()
{
    if (!m_getOverlay || !m_setOverlay)
        return {};
    return { PowerProfile::PowerSaver, PowerProfile::Balanced, PowerProfile::Performance };
}

std::optional<PowerProfile> WinSystemBattery::currentProfile()
{
    if (!m_getOverlay)
        return std::nullopt;
    GUID g{};
    if (m_getOverlay(&g) != 0)
        return std::nullopt;
    if (IsEqualGUID(g, kOverlayPowerSaver))  return PowerProfile::PowerSaver;
    if (IsEqualGUID(g, kOverlayPerformance)) return PowerProfile::Performance;
    if (IsEqualGUID(g, kOverlayBalanced) || IsEqualGUID(g, kOverlayBetterPerf))
        return PowerProfile::Balanced;
    return std::nullopt;
}

bool WinSystemBattery::setProfile(PowerProfile profile)
{
    return m_setOverlay && m_setOverlay(overlayFor(profile)) == 0;
}