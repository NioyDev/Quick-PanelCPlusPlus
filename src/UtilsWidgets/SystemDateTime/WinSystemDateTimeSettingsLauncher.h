#pragma once

#include "Interfaz/ISystemDateTimeSettingsLauncher.h"

class WinSystemDateTimeSettingsLauncher : public ISystemDateTimeSettingsLauncher {
public:
    bool openDateTimeSettings() override;
};