#pragma once

#include "Interfaz/ISystemBattery.h"

#include <QLibrary>

// Usa GetSystemPowerStatus + IOCTL de la clase Battery (SetupAPI) para
// salud/consumo/temperatura, y los "overlay schemes" de powrprof.dll
// para el perfil de rendimiento.
class ControlBatteryWindows : public IControlBattery {
public:
    ControlBatteryWindows();

    BatteryInfo readBattery() override;
    QList<PowerProfile> availableProfiles() override;
    std::optional<PowerProfile> currentProfile() override;
    bool setProfile(PowerProfile profile) override;

private:
    using GetOverlayFn = unsigned long(__stdcall*)(void* guidOut);
    using SetOverlayFn = unsigned long(__stdcall*)(struct _GUID guid);

    QLibrary m_powrprof;
    GetOverlayFn m_getOverlay = nullptr;
    SetOverlayFn m_setOverlay = nullptr;
};