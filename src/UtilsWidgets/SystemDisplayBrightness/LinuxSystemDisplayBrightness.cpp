#include "LinuxSystemDisplayBrightness.h"


#include <QProcess>
#include <QString>
#include <QStringList>
#include <algorithm>

namespace {

    constexpr int kProcessTimeoutMs = 1000;

    // Ejecuta `brightnessctl <args>` y devuelve su salida numérica.
    std::optional<double> runBrightnessctlNumber(const QStringList& args)
    {
        QProcess process;
        process.start(QStringLiteral("brightnessctl"), args);
        if (!process.waitForFinished(kProcessTimeoutMs) ||
            process.exitStatus() != QProcess::NormalExit ||
            process.exitCode() != 0) {
            return std::nullopt;
        }

        bool ok = false;
        const double value =
            QString::fromUtf8(process.readAllStandardOutput()).trimmed().toDouble(&ok);
        if (!ok)
            return std::nullopt;
        return value;
    }

} // namespace

bool LinuxSystemDisplayBrightness::isAvailable() const
{
    const auto max = runBrightnessctlNumber({ QStringLiteral("max") });
    return max.has_value() && *max > 0.0;
}

std::optional<int> LinuxSystemDisplayBrightness::brightnessPercent() const
{
    const auto current = runBrightnessctlNumber({ QStringLiteral("get") });
    const auto max = runBrightnessctlNumber({ QStringLiteral("max") });
    if (!current || !max || *max <= 0.0)
        return std::nullopt;

    return std::clamp(static_cast<int>((*current / *max) * 100.0), 0, 100);
}

bool LinuxSystemDisplayBrightness::setBrightnessPercent(int percent)
{
    percent = std::clamp(percent, 0, 100);
    // No bloquea (equivalente a subprocess.Popen del script original).
    return QProcess::startDetached(
        QStringLiteral("brightnessctl"),
        { QStringLiteral("set"), QStringLiteral("%1%").arg(percent) });
}
