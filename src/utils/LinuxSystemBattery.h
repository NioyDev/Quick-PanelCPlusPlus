#pragma once

#include "Interfaz/ISystemBattery.h"

#include <QString>
#include <optional>

// Lee /sys/class/power_supply y usa powerprofilesctl (power-profiles-daemon).
class LinuxSystemBattery : public ISystemBattery {
public:
    LinuxSystemBattery();

    BatteryInfo readBattery() override;
    QList<PowerProfile> availableProfiles() override;
    std::optional<PowerProfile> currentProfile() override;
    bool setProfile(PowerProfile profile) override;

private:
    static QString findBatteryPath();
    std::optional<QString> readText(const QString& node) const;
    std::optional<double> readNumber(const QString& node) const;
    static std::optional<QString> runPowerProfiles(const QStringList& args);

    QString m_batteryPath;
};