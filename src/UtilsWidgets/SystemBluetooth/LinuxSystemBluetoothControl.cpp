#include "LinuxSystemBluetoothControl.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>

bool LinuxSystemBluetoothControl::runBluetoothctl(const QStringList& args, QString* output, int timeoutMs) const {
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C");
    process.setProcessEnvironment(env);
    process.start("bluetoothctl", args);

    if (!process.waitForStarted(2000))
        return false;

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(500);
        return false;
    }

    if (output)
        *output = QString::fromUtf8(process.readAllStandardOutput());

    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool LinuxSystemBluetoothControl::isPowered() {
    QString out;
    if (!runBluetoothctl({ "show" }, &out))
        return false;
    return out.contains("Powered: yes");
}

bool LinuxSystemBluetoothControl::setPowered(bool enabled) {
    return runBluetoothctl({ "power", enabled ? "on" : "off" });
}

QStringList LinuxSystemBluetoothControl::connectedMacs() const {
    QString out;
    QStringList macs;
    if (!runBluetoothctl({ "devices", "Connected" }, &out))
        return macs;

    for (const QString& line : out.split('\n')) {
        if (!line.startsWith("Device "))
            continue;
        const QStringList parts = line.split(' ');
        if (parts.size() >= 2)
            macs << parts[1];
    }
    return macs;
}

QList<BluetoothDeviceInfo> LinuxSystemBluetoothControl::knownDevices() {
    QList<BluetoothDeviceInfo> devices;
    const QSet<QString> connected = QSet<QString>(connectedMacs().begin(), connectedMacs().end());

    QString out;
    if (!runBluetoothctl({ "devices" }, &out))
        return devices;

    for (const QString& line : out.split('\n')) {
        if (!line.startsWith("Device "))
            continue;
        const QStringList parts = line.split(' ');
        if (parts.size() < 3)
            continue;

        BluetoothDeviceInfo info;
        info.mac = parts[1];
        info.name = line.section(' ', 2).trimmed();
        info.connected = connected.contains(info.mac);
        devices.append(info);
    }
    return devices;
}

bool LinuxSystemBluetoothControl::connectDevice(const QString& mac) {
    return runBluetoothctl({ "connect", mac }, nullptr, 20000);
}

bool LinuxSystemBluetoothControl::disconnectDevice(const QString& mac) {
    return runBluetoothctl({ "disconnect", mac }, nullptr, 10000);
}

bool LinuxSystemBluetoothControl::openBluetoothSettings() {
    return QProcess::startDetached("blueman-manager", {});
}