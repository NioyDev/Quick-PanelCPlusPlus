#pragma once

#include <QDate>
#include <QWidget>
#include <memory>

#include "Interfaz/ISystemDateTimeSettingsLauncher.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ClockCalendarPopupWindow; }
QT_END_NAMESPACE

class QTimer;

class ClockCalendarPopupWindow : public QWidget {
    Q_OBJECT

public:
    explicit ClockCalendarPopupWindow(
        std::shared_ptr<ISystemDateTimeSettingsLauncher> settingsLauncher,
        QWidget* parent = nullptr);
    ~ClockCalendarPopupWindow() override;

    // Métodos públicos estáticos para obtener fecha y hora formateadas sin duplicar lógica
    static QString detectDateFormat();
    static QString getFormattedTime12h(bool includeSeconds = false);
    static QString getFormattedDate();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void updateClock();
    void onDateTimeSettingsClicked();

private:
    void applyStyle();
    void positionWindow();

    Ui::ClockCalendarPopupWindow* ui;
    std::shared_ptr<ISystemDateTimeSettingsLauncher> m_settingsLauncher;
    QTimer* m_clockTimer = nullptr;
    QString m_dateFormat;   // "dd/MM/yyyy", "MM/dd/yyyy" o "yyyy/MM/dd"
    QDate m_lastDate;
};