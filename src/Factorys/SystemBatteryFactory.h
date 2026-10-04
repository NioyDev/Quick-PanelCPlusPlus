#pragma once

#include "Interfaz/ISystemBattery.h"
#include "Utils&Widgets/SystemBattery/LinuxSystemBattery.h"
#include "Utils&Widgets/SystemBattery/WinSystembattery.h"

#include <memory>

class QuickBatteryWindow;

class ControlBatteryFactory {
public:
    static std::shared_ptr<ISystemBattery> create() {
#ifdef _WIN32
        return std::make_shared<WinSystemBattery>();
#else
        return std::make_shared<ControlBatteryLinux>();
#endif
    }
};