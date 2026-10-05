#pragma once

#include "Interfaz/ISystemBluetoothControl.h"
#include <QStringList>

class LinuxSystemBluetoothControl : public ISystemBluetoothControl {
public:
    bool isPowered() override;
    bool setPowered(bool enabled) override;
    QList<BluetoothDeviceInfo> knownDevices() override;
    bool connectDevice(const QString& mac) override;
    bool disconnectDevice(const QString& mac) override;
    bool openBluetoothSettings() override;

private:
    bool runBluetoothctl(const QStringList& args, QString* output = nullptr, int timeoutMs = 5000) const;
    QStringList connectedMacs() const;
};