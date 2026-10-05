#include "WinSystemDateTimeSettingsLauncher.h"

#include <QDesktopServices>
#include <QProcess>
#include <QUrl>

bool WinSystemDateTimeSettingsLauncher::openDateTimeSettings() {
    // Windows 10/11: Configuración > Hora e idioma > Fecha y hora
    if (QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:dateandtime")))) {
        return true;
    }
    // Respaldo clásico: Panel de control > Fecha y hora
    return QProcess::startDetached(QStringLiteral("control.exe"),
        { QStringLiteral("timedate.cpl") });
}