#pragma once

#include "Interfaz/ISystemBluetoothControl.h"

#include <QHash>
#include <QSet>
#include <QTimer>      // NUEVO
#include <QWidget>
#include <memory>

class QPushButton;
class QKeyEvent;
class QCloseEvent;
class QShowEvent;      // NUEVO
class QHideEvent;      // NUEVO

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
    void showEvent(QShowEvent* event) override;   
    void hideEvent(QHideEvent* event) override;   

private slots:
    void onPowerToggled(bool enabled);
    void onSettingsClicked();
    void onRestartAdapterClicked();

private:
    struct Snapshot {
        bool powered = false;
        QList<BluetoothDeviceInfo> devices;
    };

    enum class ActionResult { Success, Failed };   

    void refreshAsync(bool force = false);                           
    void onSnapshotReady(const Snapshot& snapshot, bool force);      
    void applySnapshot(const Snapshot& snapshot);
    void clearDeviceList();
    void addDeviceRow(const BluetoothDeviceInfo& device);
    void runDeviceAction(QPushButton* button, const QString& mac, bool connect);
    void onDeviceActionFinished(const QString& mac, bool connectAction, ActionResult result);   

    Ui::QuickBluetoothWindow* ui;
    std::shared_ptr<ISystemBluetoothControl> m_control;
    bool m_ignorePowerSignal = true;

    QSet<QString> m_pendingMacs;               
    QHash<QString, QString> m_rowMessages;     

    QTimer* m_pollTimer = nullptr;             
    Snapshot m_lastSnapshot;        
    QPushButton* m_restartButton = nullptr;
    bool m_hasSnapshot = false;                
    bool m_refreshInFlight = false;            
    bool m_refreshQueued = false;              
    bool m_queuedForce = false;                
    bool m_powerChangePending = false;         
};