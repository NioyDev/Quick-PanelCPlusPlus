#pragma once

#include "Interfaz/ISystemBluetoothControl.h"


class WinSystemBluetoothControl : public ISystemBluetoothControl {
public:
    bool isPowered() override;
    bool setPowered(bool enabled) override;
    QList<BluetoothDeviceInfo> knownDevices() override;
    bool connectDevice(const QString& mac) override;
    bool disconnectDevice(const QString& mac) override;
    bool openBluetoothSettings() override;
};