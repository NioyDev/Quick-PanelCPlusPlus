#include "LinuxSystemBattery.h"

#include <QDir>
#include <QFile>
#include <QProcess>

#include <algorithm>
#include <cmath>

namespace {
    constexpr int kProcessTimeoutMs = 2000;

    QString profileId(PowerProfile p)
    {
        switch (p) {
        case PowerProfile::PowerSaver:  return QStringLiteral("power-saver");
        case PowerProfile::Balanced:    return QStringLiteral("balanced");
        case PowerProfile::Performance: return QStringLiteral("performance");
        }
        return {};
    }
} // namespace

ControlBatteryLinux::ControlBatteryLinux()
    : m_batteryPath(findBatteryPath())
{}

QString ControlBatteryLinux::findBatteryPath()
{
    const QDir root(QStringLiteral("/sys/class/power_supply"));
    const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& name : entries) {
        const QString base = root.filePath(name);
        auto read = [&](const char* node) {
            QFile f(base + QLatin1Char('/') + QLatin1String(node));
            return f.open(QIODevice::ReadOnly | QIODevice::Text)
                ? QString::fromUtf8(f.readAll()).trimmed() : QString();
            };
        // "Device" = baterías de periféricos (mouse, teclado...), se ignoran.
        if (read("type") == QLatin1String("Battery") && read("scope") != QLatin1String("Device"))
            return base;
    }
    return {};
}

std::optional<QString> ControlBatteryLinux::readText(const QString& node) const
{
    if (m_batteryPath.isEmpty())
        return std::nullopt;
    QFile f(m_batteryPath + QLatin1Char('/') + node);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return std::nullopt;
    return QString::fromUtf8(f.readAll()).trimmed();
}

std::optional<double> ControlBatteryLinux::readNumber(const QString& node) const
{
    const auto text = readText(node);
    if (!text)
        return std::nullopt;
    bool ok = false;
    const double v = text->toDouble(&ok);
    return ok ? std::optional<double>(v) : std::nullopt;
}

BatteryInfo ControlBatteryLinux::readBattery()
{
    BatteryInfo info;
    const auto capacity = readNumber(QStringLiteral("capacity"));
    if (!capacity)
        return info;

    info.present = true;
    info.capacityPercent = static_cast<int>(*capacity);

    if (const auto status = readText(QStringLiteral("status"))) {
        if (*status == QLatin1String("Charging"))         info.state = BatteryState::Charging;
        else if (*status == QLatin1String("Discharging")) info.state = BatteryState::Discharging;
        else if (*status == QLatin1String("Full"))        info.state = BatteryState::Full;
        else if (*status == QLatin1String("Not charging")) info.state = BatteryState::NotCharging;
    }

    // Algunas laptops exponen charge_* (µAh) y otras energy_* (µWh).
    const bool useCharge = readNumber(QStringLiteral("charge_full_design")).has_value()
        || readNumber(QStringLiteral("charge_now")).has_value();
    const QString unit = useCharge ? QStringLiteral("charge") : QStringLiteral("energy");

    const auto fullDesign = readNumber(unit + QStringLiteral("_full_design"));
    const auto fullNow = readNumber(unit + QStringLiteral("_full"));
    const auto now = readNumber(unit + QStringLiteral("_now"));
    const auto voltage = readNumber(QStringLiteral("voltage_now"));
    const auto current = readNumber(QStringLiteral("current_now"));
    const auto powerNow = readNumber(QStringLiteral("power_now"));

    if (fullDesign && fullNow && *fullDesign > 0)
        info.healthPercent = (*fullNow / *fullDesign) * 100.0;

    if (powerNow)
        info.powerWatts = std::abs(*powerNow) / 1e6;
    else if (voltage && current)
        info.powerWatts = std::abs((*voltage / 1e6) * (*current / 1e6));

    if (const auto temp = readNumber(QStringLiteral("temp")))
        info.temperatureCelsius = *temp / 10.0;

    // Tasa en la misma unidad que "now" por hora (µA con µAh, µW con µWh).
    const double rate = std::abs(useCharge ? current.value_or(0.0) : powerNow.value_or(0.0));
    if (rate > 0.0 && now) {
        std::optional<double> remaining;
        if (info.state == BatteryState::Charging && fullNow)
            remaining = std::max(0.0, *fullNow - *now);
        else if (info.state == BatteryState::Discharging)
            remaining = *now;
        if (remaining)
            info.remainingMinutes = static_cast<int>((*remaining / rate) * 60.0);
    }
    return info;
}

std::optional<QString> ControlBatteryLinux::runPowerProfiles(const QStringList& args)
{
    QProcess p;
    p.start(QStringLiteral("powerprofilesctl"), args);
    if (!p.waitForStarted(kProcessTimeoutMs) || !p.waitForFinished(kProcessTimeoutMs))
        return std::nullopt;
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0)
        return std::nullopt;
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

QList<PowerProfile> ControlBatteryLinux::availableProfiles()
{
    const auto out = runPowerProfiles({ QStringLiteral("list") });
    QList<PowerProfile> result;
    if (!out)
        return result;
    for (PowerProfile p : {PowerProfile::PowerSaver, PowerProfile::Balanced, PowerProfile::Performance}) {
        if (out->contains(profileId(p)))
            result.append(p);
    }
    return result;
}

std::optional<PowerProfile> ControlBatteryLinux::currentProfile()
{
    const auto out = runPowerProfiles({ QStringLiteral("get") });
    if (!out)
        return std::nullopt;
    for (PowerProfile p : {PowerProfile::PowerSaver, PowerProfile::Balanced, PowerProfile::Performance}) {
        if (*out == profileId(p))
            return p;
    }
    return std::nullopt;
}

bool ControlBatteryLinux::setProfile(PowerProfile profile)
{
    return runPowerProfiles({ QStringLiteral("set"), profileId(profile) }).has_value();
}