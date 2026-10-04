#include "QuickBatteryWindow.h"
#include "ui_quickbatterywindow.h"

#include <QScreen>
#include <QStyle>
#include <QTimer>
#include <QKeyEvent>

namespace {
    constexpr int kLowBatteryPercent = 20;
    constexpr int kRefreshIntervalMs = 5000;
}

QuickBatteryWindow::QuickBatteryWindow(std::shared_ptr<ISystemBattery> battery, QWidget* parent)
    : QWidget(parent)
    , m_ui(std::make_unique<Ui::QuickBatteryWindow>())
    , m_battery(std::move(battery))
{
    m_ui->setupUi(this);

    // Ventana tipo Tool sin marco y siempre visible
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    // Conectar botones de perfiles de rendimiento
    connect(m_ui->buttonPowerSaver, &QPushButton::clicked, this, &QuickBatteryWindow::onPowerSaverClicked);
    connect(m_ui->buttonBalanced, &QPushButton::clicked, this, &QuickBatteryWindow::onBalancedClicked);
    connect(m_ui->buttonPerformance, &QPushButton::clicked, this, &QuickBatteryWindow::onPerformanceClicked);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &QuickBatteryWindow::refresh);
    m_timer->start(kRefreshIntervalMs);

    refresh();
}

QuickBatteryWindow::~QuickBatteryWindow() = default;

void QuickBatteryWindow::enterEvent(QEnterEvent* event) {
    emit mouseEnteredWindow();
    QWidget::enterEvent(event);
}

void QuickBatteryWindow::leaveEvent(QEvent* event) {
    emit mouseLeftWindow();
    QWidget::leaveEvent(event);
}

void QuickBatteryWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    adjustSize();
}

void QuickBatteryWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    QWidget::keyPressEvent(event);
}

void QuickBatteryWindow::onPowerSaverClicked() {
    if (m_battery) {
        m_battery->setProfile(PowerProfile::PowerSaver);
        refresh();
    }
}

void QuickBatteryWindow::onBalancedClicked() {
    if (m_battery) {
        m_battery->setProfile(PowerProfile::Balanced);
        refresh();
    }
}

void QuickBatteryWindow::onPerformanceClicked() {
    if (m_battery) {
        m_battery->setProfile(PowerProfile::Performance);
        refresh();
    }
}

void QuickBatteryWindow::refresh() {
    if (m_battery) {
        applyBattery(m_battery->readBattery());
    }
}

void QuickBatteryWindow::updateProfileButtons(PowerProfile currentProfile) {
    m_ui->buttonPowerSaver->setChecked(currentProfile == PowerProfile::PowerSaver);
    m_ui->buttonBalanced->setChecked(currentProfile == PowerProfile::Balanced);
    m_ui->buttonPerformance->setChecked(currentProfile == PowerProfile::Performance);
}

void QuickBatteryWindow::applyBattery(const BatteryInfo& info) {
    const QString dash = QStringLiteral("-");

    if (!info.present) {
        m_ui->labelTitle->setText(tr("Batería"));
        m_ui->progressBattery->setValue(0);
        m_ui->labelStatus->setText(tr("Batería no detectada"));
        m_ui->labelHealthValue->setText(dash);
        m_ui->labelPowerValue->setText(dash);
        m_ui->labelTempValue->setText(dash);
        m_ui->labelTimeValue->setText(dash);
        return;
    }

    m_ui->labelTitle->setText(tr("Batería %1%").arg(info.capacityPercent));
    m_ui->progressBattery->setValue(info.capacityPercent);

    const bool low = info.capacityPercent <= kLowBatteryPercent;
    QProgressBar* bar = m_ui->progressBattery;
    if (bar->property("low").toBool() != low) {
        bar->setProperty("low", low);
        bar->style()->unpolish(bar);
        bar->style()->polish(bar);
    }

    switch (info.state) {
    case BatteryState::Charging:
        m_ui->labelStatus->setText(tr("<span style='color:#10b981'>⚡ Cargando</span>")); break;
    case BatteryState::Discharging:
        m_ui->labelStatus->setText(tr("<span style='color:#eab308'>🔋 Descargando</span>")); break;
    case BatteryState::Full:
        m_ui->labelStatus->setText(tr("<span style='color:#3b82f6'>✅ Completamente cargada</span>")); break;
    case BatteryState::NotCharging:
        m_ui->labelStatus->setText(tr("<span style='color:#3b82f6'>🔌 Conectada, sin cargar</span>")); break;
    case BatteryState::Unknown:
        m_ui->labelStatus->setText(tr("Estado desconocido")); break;
    }

    if (info.healthPercent) {
        const QString h = QString::number(*info.healthPercent, 'f', 1);
        m_ui->labelHealthValue->setText(*info.healthPercent < 50.0
            ? tr("<span style='color:#ef4444'>%1% (Degradada)</span>").arg(h)
            : QStringLiteral("%1%").arg(h));
    }
    else {
        m_ui->labelHealthValue->setText(dash);
    }

    m_ui->labelPowerValue->setText(info.powerWatts
        ? QStringLiteral("%1 W").arg(*info.powerWatts, 0, 'f', 1) : dash);

    m_ui->labelTempValue->setText(info.temperatureCelsius
        ? QStringLiteral("%1 °C").arg(*info.temperatureCelsius, 0, 'f', 1) : dash);

    if (info.remainingMinutes) {
        m_ui->labelTimeValue->setText(
            QStringLiteral("%1h %2m").arg(*info.remainingMinutes / 60).arg(*info.remainingMinutes % 60));
    }
    else {
        m_ui->labelTimeValue->setText(tr("Calculando..."));
    }

    // Consultar el perfil actual mediante m_battery->currentProfile()
    if (m_battery) {
        auto activeProfile = m_battery->currentProfile();
        if (activeProfile.has_value()) {
            updateProfileButtons(*activeProfile);
        }
    }
}