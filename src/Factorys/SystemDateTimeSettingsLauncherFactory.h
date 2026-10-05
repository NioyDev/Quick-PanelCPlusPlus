#pragma once

#include "Interfaz/ISystemDateTimeSettingsLauncher.h"
#include <memory>

#ifdef _WIN32
#include "UtilsWidgets/SystemDateTime/WinSystemDateTimeSettingsLauncher.h"
#else
#include "UtilsWidgets/SystemDateTime/LinuxSystemDateTimeSettingsLauncher.h"
#endif

class SystemDateTimeSettingsLauncherFactory {
public:
    static std::shared_ptr<ISystemDateTimeSettingsLauncher> create() {
#ifdef _WIN32
        return std::make_shared<WinSystemDateTimeSettingsLauncher>();
#else
        return std::make_shared<LinuxSystemDateTimeSettingsLauncher>();
#endif
    }
};