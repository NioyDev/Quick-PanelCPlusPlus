#pragma once

#include "Interfaz/ISystemBattery.h"
#include "utils/LinuxSystemBattery.h"
#include "utils/WinSystembattery.h"

#include <memory>

class QuickBatteryWindow;

class ControlBatteryFactory {
public:
    static std::shared_ptr<IControlBattery> create() {
#ifdef _WIN32
        return std::make_shared<ControlBatteryWindows>();
#else
        return std::make_shared<ControlBatteryLinux>();
#endif
    }
};