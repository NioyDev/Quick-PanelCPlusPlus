#pragma once

#include <QList>
#include <optional>

enum class BatteryState { Unknown, Charging, Discharging, Full, NotCharging };
enum class PowerProfile { PowerSaver, Balanced, Performance };

struct BatteryInfo {
    bool present = false;
    int capacityPercent = 0;
    BatteryState state = BatteryState::Unknown;
    std::optional<double> healthPercent;
    std::optional<double> powerWatts;
    std::optional<double> temperatureCelsius;
    std::optional<int> remainingMinutes;
};

// Contrato que implementan ControlBatteryLinux y ControlBatteryWindows.
// No hereda de QObject porque no emite señales; si luego se quiere
// reaccionar a eventos del sistema, basta con heredar de QObject aquí.
class IControlBattery {
public:
    explicit IControlBattery() {};
    virtual ~IControlBattery() = default;

    virtual BatteryInfo readBattery() = 0;
    virtual QList<PowerProfile> availableProfiles() = 0;
    virtual std::optional<PowerProfile> currentProfile() = 0;
    virtual bool setProfile(PowerProfile profile) = 0;
};