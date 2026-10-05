#pragma once

// Abre el panel de ajustes de fecha/hora/calendario del sistema operativo.
class ISystemDateTimeSettingsLauncher {
public:
    virtual ~ISystemDateTimeSettingsLauncher() = default;

    // Devuelve true si se logró lanzar algún panel/herramienta de ajustes.
    virtual bool openDateTimeSettings() = 0;
};