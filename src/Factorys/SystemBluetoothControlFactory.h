#pragma once

#include "Interfaz/ISystemBluetoothControl.h"
#include <memory>

#ifdef _WIN32
#include "UtilsWidgets/SystemBluetooth/WinSystemBluetoothControl.h"
#else
#include "UtilsWidgets/SystemBluetooth/LinuxSystemBluetoothControl.h"
#endif

class SystemBluetoothControlFactory {
public:
    static std::shared_ptr<ISystemBluetoothControl> create() {
#ifdef _WIN32
        return std::make_shared<WinSystemBluetoothControl>();
#else
        return std::make_shared<LinuxSystemBluetoothControl>();
#endif
    }
};