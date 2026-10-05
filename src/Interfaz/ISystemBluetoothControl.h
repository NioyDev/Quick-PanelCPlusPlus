#pragma once

#include <QList>
#include <QString>

struct BluetoothDeviceInfo {
    QString mac;
    QString name;
    bool connected = false;
};

class ISystemBluetoothControl {
public:
    virtual ~ISystemBluetoothControl() = default;

    virtual bool isPowered() = 0;
    virtual bool setPowered(bool enabled) = 0;
    virtual QList<BluetoothDeviceInfo> knownDevices() = 0;
    virtual bool connectDevice(const QString& mac) = 0;
    virtual bool disconnectDevice(const QString& mac) = 0;
    virtual bool openBluetoothSettings() = 0;
};