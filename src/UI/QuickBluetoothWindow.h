#pragma once

#include "Interfaz/ISystemBluetoothControl.h"

#include <QWidget>
#include <memory>

class QPushButton;
class QKeyEvent;
class QCloseEvent;

namespace Ui {
    class QuickBluetoothWindow;
}

class QuickBluetoothWindow : public QWidget {
    Q_OBJECT

public:
    explicit QuickBluetoothWindow(std::shared_ptr<ISystemBluetoothControl> control, QWidget* parent = nullptr);
    ~QuickBluetoothWindow() override;

    void showAtBottomRight();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onPowerToggled(bool enabled);
    void onSettingsClicked();

private:
    enum class ActionResult { Success, Failed };
    struct Snapshot {
        bool powered = false;
        QList<BluetoothDeviceInfo> devices;
    };
    QSet<QString> m_pendingMacs;
    QHash<QString, QString> m_rowMessages;

    void onDeviceActionFinished(const QString& mac, bool connectAction, ActionResult result);
    void refreshAsync();
    void applySnapshot(const Snapshot& snapshot);
    void clearDeviceList();
    void addDeviceRow(const BluetoothDeviceInfo& device);
    void runDeviceAction(QPushButton* button, const QString& mac, bool connect);

    Ui::QuickBluetoothWindow* ui;
    std::shared_ptr<ISystemBluetoothControl> m_control;
    bool m_ignorePowerSignal = true;
};