#pragma once

#include <QMainWindow>
#include <QTimer>
#include <memory>
#include "Interfaz/ISystemControl.h"
#include "Interfaz/ISystemKey.h"
#include "Interfaz/ISystemBattery.h"
#include "Interfaz/ISystemBluetoothControl.h"
#include "Interfaz/ISystemDisplayBrightness.h"
#include "Interfaz/ISystemDateTimeSettingsLauncher.h"
#include "CompatWindow.h"
#include "KeySequenceWindow.h"
#include "QuickBatteryWindow.h"
#include "QuickBluetoothWindow.h"
#include "QuickBrightnessWindow.h"
#include "ClockCalendarPopupWindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(std::shared_ptr<ISystemControl> systemControl,
        std::shared_ptr<ISystemKey> systemKey, std::shared_ptr<ISystemBattery> systemBattery,
        std::shared_ptr<ISystemBluetoothControl> systemBluetooth, std::shared_ptr<ISystemDisplayBrightness> systemDisplayBrightness,
        std::shared_ptr<ISystemDateTimeSettingsLauncher> systemDateTimeSettings,
        QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void updateBatteryStatus();
    void updateClockButton();
    void onBatteryClicked();
    void onBluetoothClicked();
    void onBrightnessClicked();
    void onClockClicked();

private:
    void updateBluetoothWindowPosition();
    void updateBatteryWindowPosition();
    void updateBrightnessWindowPosition();
    void updateClockWindowPosition();
    void setupPanelGeometry(QWidget* widget, double widthRatio = 0.90, double heightRatio = 0.05);

    Ui::MainWindow* ui;
    std::shared_ptr<ISystemControl> m_systemControl;
    std::shared_ptr<ISystemKey> m_systemKey;
    std::shared_ptr<ISystemBattery> m_systemBattery;
    std::shared_ptr<ISystemBluetoothControl> m_systemBluetooth;
    std::shared_ptr<ISystemDisplayBrightness> m_systemDisplayBrightness;
    std::shared_ptr<ISystemDateTimeSettingsLauncher> m_systemDateTimeSettings;

    // Ventanas emergentes
    std::unique_ptr<CompatWindow> m_compatWindow;
    std::unique_ptr<KeySequenceWindow> m_keySequenceWindow;
    std::unique_ptr<QuickBatteryWindow> m_QuickBatteryWindow;
    std::unique_ptr<QuickBluetoothWindow> m_QuickBluetoothWindow;
    std::unique_ptr<QuickBrightnessWindow> m_QuickBrightnessWindow;
    std::unique_ptr<ClockCalendarPopupWindow> m_ClockCalendarPopupWindow;

    QTimer* m_batteryTimer = nullptr;
    QTimer* m_batteryHideTimer = nullptr;
    QTimer* m_clockTimer = nullptr;
};