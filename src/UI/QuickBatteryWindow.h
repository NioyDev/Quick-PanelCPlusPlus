#pragma once

#include "Interfaz/ISystemBattery.h"
#include <QWidget>
#include <memory>

class QTimer;

namespace Ui { class QuickBatteryWindow; }

class QuickBatteryWindow : public QWidget {
    Q_OBJECT

public:
    explicit QuickBatteryWindow(std::shared_ptr<ISystemBattery> battery, QWidget* parent = nullptr);
    ~QuickBatteryWindow() override;

signals:
    void mouseEnteredWindow();
    void mouseLeftWindow();

protected:
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onPowerSaverClicked();
    void onBalancedClicked();
    void onPerformanceClicked();

private:
    void refresh();
    void applyBattery(const BatteryInfo& info);
    void updateProfileButtons(PowerProfile currentProfile);

    std::unique_ptr<Ui::QuickBatteryWindow> m_ui;
    std::shared_ptr<ISystemBattery> m_battery;
    QTimer* m_timer = nullptr;
};