#include "LinuxSystemDateTimeSettingsLauncher.h"

#include <QList>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

namespace {

    struct Command {
        QString program;
        QStringList args;
    };

    // Comandos conocidos por entorno de escritorio. Se prueba primero el del
    // escritorio actual y luego el resto como respaldo.
    QList<Command> buildCandidates(const QString& desktop) {
        const Command gnome{ "gnome-control-center", {"datetime"} };
        const Command plasma6{ "kcmshell6", {"kcm_clock"} };
        const Command plasma5{ "kcmshell5", {"clock"} };
        const Command cinnamon{ "cinnamon-settings", {"calendar"} };
        const Command mate{ "time-admin", {} };
        const Command budgie{ "budgie-control-center", {"datetime"} };
        const Command lxqt{ "lxqt-admin-time", {} };
        const Command xfce{ "xfce4-settings-manager", {} };

        QList<Command> list;
        if (desktop.contains("gnome") || desktop.contains("unity") || desktop.contains("ubuntu")) list << gnome;
        if (desktop.contains("kde") || desktop.contains("plasma")) list << plasma6 << plasma5;
        if (desktop.contains("cinnamon")) list << cinnamon;
        if (desktop.contains("mate"))     list << mate;
        if (desktop.contains("budgie"))   list << budgie;
        if (desktop.contains("lxqt"))     list << lxqt;
        if (desktop.contains("xfce"))     list << xfce;

        // Respaldo: cualquiera que exista en el sistema
        list << gnome << plasma6 << plasma5 << cinnamon << mate << budgie << lxqt << xfce;
        return list;
    }

} // namespace

bool LinuxSystemDateTimeSettingsLauncher::openDateTimeSettings() {
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower();

    for (const Command& cmd : buildCandidates(desktop)) {
        if (QStandardPaths::findExecutable(cmd.program).isEmpty()) {
            continue;
        }
        if (QProcess::startDetached(cmd.program, cmd.args)) {
            return true;
        }
    }
    return false;
}