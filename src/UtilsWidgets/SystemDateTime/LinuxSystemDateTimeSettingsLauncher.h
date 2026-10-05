#pragma once

#include "Interfaz/ISystemDateTimeSettingsLauncher.h"

class LinuxSystemDateTimeSettingsLauncher : public ISystemDateTimeSettingsLauncher {
public:
    bool openDateTimeSettings() override;
};