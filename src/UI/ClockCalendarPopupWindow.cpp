#include "ClockCalendarPopupWindow.h"
#include "ui_clockcalendarpopupwindow.h"

#include <QCalendarWidget>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLocale>
#include <QScreen>
#include <QTextCharFormat>
#include <QTime>
#include <QTimer>
#include <algorithm>

namespace {
    constexpr int kMarginRight = 20;
    constexpr int kMarginBottom = 60;
}

ClockCalendarPopupWindow::ClockCalendarPopupWindow(
    std::shared_ptr<ISystemDateTimeSettingsLauncher> settingsLauncher,
    QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ClockCalendarPopupWindow)
    , m_settingsLauncher(std::move(settingsLauncher))
    , m_dateFormat(detectDateFormat())
{
    ui->setupUi(this);

    // Popup: se cierra solo al hacer clic fuera. Sin borde, fondo transparente.
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    auto* shadow = new QGraphicsDropShadowEffect(ui->frameCard);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 128));
    ui->frameCard->setGraphicsEffect(shadow);

    ui->calendarWidget->setLocale(QLocale::system());
    ui->calendarWidget->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    ui->calendarWidget->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
    ui->calendarWidget->setSelectedDate(QDate::currentDate());

    // Fines de semana con el mismo color que el resto de días
    QTextCharFormat normal;
    normal.setForeground(QColor("#fafafa"));
    ui->calendarWidget->setWeekdayTextFormat(Qt::Saturday, normal);
    ui->calendarWidget->setWeekdayTextFormat(Qt::Sunday, normal);

    applyStyle();

    connect(ui->btnDateTimeSettings, &QPushButton::clicked,
        this, &ClockCalendarPopupWindow::onDateTimeSettingsClicked);

    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &ClockCalendarPopupWindow::updateClock);
    m_clockTimer->start(1000);
    updateClock();
}

ClockCalendarPopupWindow::~ClockCalendarPopupWindow() {
    delete ui;
}

// Ordena día/mes/año según el formato corto de la configuración regional.
// EE.UU. -> MM/dd/yyyy, Asia -> yyyy/MM/dd, resto -> dd/MM/yyyy.
QString ClockCalendarPopupWindow::detectDateFormat() {
    const QString f = QLocale::system().dateFormat(QLocale::ShortFormat);

    QList<QPair<int, QString>> parts = {
        {f.indexOf('y'), QStringLiteral("yyyy")},
        {f.indexOf('M'), QStringLiteral("MM")},
        {f.indexOf('d'), QStringLiteral("dd")},
    };

    const bool allFound = std::all_of(parts.begin(), parts.end(),
        [](const auto& p) { return p.first >= 0; });
    if (!allFound) {
        return QStringLiteral("dd/MM/yyyy");
    }

    std::sort(parts.begin(), parts.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    return parts[0].second + '/' + parts[1].second + '/' + parts[2].second;
}

QString ClockCalendarPopupWindow::getFormattedTime12h(bool includeSeconds) {
    const QTime t = QTime::currentTime();
    int hour12 = t.hour() % 12;
    if (hour12 == 0) hour12 = 12;
    const QString ampm = t.hour() < 12 ? QStringLiteral("AM") : QStringLiteral("PM");

    if (includeSeconds) {
        return QStringLiteral("%1:%2:%3 %4")
            .arg(hour12)
            .arg(t.minute(), 2, 10, QChar('0'))
            .arg(t.second(), 2, 10, QChar('0'))
            .arg(ampm);
    }

    return QStringLiteral("%1:%2 %3")
        .arg(hour12)
        .arg(t.minute(), 2, 10, QChar('0'))
        .arg(ampm);
}

QString ClockCalendarPopupWindow::getFormattedDate() {
    return QDate::currentDate().toString(detectDateFormat());
}

void ClockCalendarPopupWindow::updateClock() {
    const QDateTime now = QDateTime::currentDateTime();

    ui->lblTime->setText(getFormattedTime12h(/*includeSeconds=*/true));
    ui->lblDate->setText(getFormattedDate());

    // Si cambió el día (medianoche), el calendario salta al día actual
    if (m_lastDate != now.date()) {
        m_lastDate = now.date();
        ui->calendarWidget->setSelectedDate(m_lastDate);
    }
}

void ClockCalendarPopupWindow::onDateTimeSettingsClicked() {
    if (m_settingsLauncher && m_settingsLauncher->openDateTimeSettings()) {
        close();
        return;
    }
    ui->btnDateTimeSettings->setText(tr("No se encontró un panel de ajustes"));
    QTimer::singleShot(2500, this, [this] {
        ui->btnDateTimeSettings->setText(tr("Ajustes de fecha y hora"));
        });
}

void ClockCalendarPopupWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ClockCalendarPopupWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    //positionWindow();
}

void ClockCalendarPopupWindow::positionWindow() {
    adjustSize();
    const QRect area = QGuiApplication::primaryScreen()->availableGeometry();
    move(area.right() - width() - kMarginRight + 1,
        area.bottom() - height() - kMarginBottom + 1);
}

void ClockCalendarPopupWindow::applyStyle() {
    setStyleSheet(R"(
        #frameCard {
            background-color: #18181b;
            border: 1px solid #27272a;
            border-radius: 20px;
        }
        QLabel { color: #fafafa; background: transparent; }
        #lblTime { font-size: 24pt; font-weight: bold; }
        #lblDate { font-size: 14pt; font-weight: bold; color: #a1a1aa; }

        #calendarWidget { background-color: #18181b; }
        #calendarWidget QWidget#qt_calendar_navigationbar { background-color: #18181b; }
        #calendarWidget QToolButton {
            color: #fafafa; background: transparent; border: none;
            font-weight: bold; padding: 4px 8px; border-radius: 6px;
        }
        #calendarWidget QToolButton:hover { background-color: #27272a; }
        #calendarWidget QToolButton::menu-indicator { image: none; }
        #calendarWidget QMenu { background-color: #18181b; color: #fafafa; }
        #calendarWidget QSpinBox { background: #18181b; color: #fafafa; border: none; }
        #calendarWidget QAbstractItemView {
            background-color: #18181b; color: #fafafa; outline: 0;
            selection-background-color: #ef4444; selection-color: #ffffff;
        }
        #calendarWidget QAbstractItemView:disabled { color: #52525b; }

        #btnDateTimeSettings {
            color: #a1a1aa; background: transparent; border: none;
            padding: 8px; border-radius: 8px;
        }
        #btnDateTimeSettings:hover { background-color: #27272a; color: #fafafa; }
    )");
}