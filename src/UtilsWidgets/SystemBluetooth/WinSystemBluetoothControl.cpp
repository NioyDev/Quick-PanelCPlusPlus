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
        dev.connected = info.fConnected;
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

    // 3. Fallback con API Win32
    BLUETOOTH_DEVICE_INFO info;
    if (loadDeviceInfo(mac, info)) {
        return setAllServices(info, BLUETOOTH_SERVICE_ENABLE);
    }

    return false;
}

bool WinSystemBluetoothControl::disconnectDevice(const QString& mac) {
    ensureWinRtApartment();

    // 1. Intentar desconexión vía Win32
    BLUETOOTH_DEVICE_INFO info;
    if (loadDeviceInfo(mac, info)) {
        setAllServices(info, BLUETOOTH_SERVICE_DISABLE);
    }

    // 2. Liberación limpia en WinRT
    try {
        using namespace winrt::Windows::Devices::Bluetooth;
        const uint64_t address = macToUint64(mac);
        if (address != 0) {
            auto device = BluetoothDevice::FromBluetoothAddressAsync(address).get();
            if (device) {
                device.Close();
            }
        }
    }
    catch (...) {
    }

    return true;
}

bool WinSystemBluetoothControl::openBluetoothSettings() {
    return QDesktopServices::openUrl(QUrl("ms-settings:bluetooth"));
}