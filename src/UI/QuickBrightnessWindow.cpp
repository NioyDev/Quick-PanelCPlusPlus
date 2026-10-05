#include "QuickBrightnessWindow.h"
#include "ui_quickbrightnesswindow.h"

#include <QGuiApplication>
#include <QHideEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QScreen>
#include <QSignalBlocker>
#include <QTimer>
#include <QtMath>

namespace {
    constexpr int kStep = 5;
    constexpr int kMarginRight = 20;
    constexpr int kMarginBottom = 80;
    constexpr int kApplyDelayMs = 16; // debounce del slider
}

QuickBrightnessWindow::QuickBrightnessWindow(std::shared_ptr<ISystemDisplayBrightness> brightness,
    QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , ui(new Ui::QuickBrightnessWindow)
    , m_brightness(std::move(brightness))
{
    ui->setupUi(this);
    setAttribute(Qt::WA_TranslucentBackground);

    m_applyTimer = new QTimer(this);
    m_applyTimer->setSingleShot(true);
    m_applyTimer->setInterval(kApplyDelayMs);
    connect(m_applyTimer, &QTimer::timeout,
        this, &QuickBrightnessWindow::applyPendingBrightness);

    setupIcon();
    loadCurrentBrightness();

    connect(ui->brightnessSlider, &QSlider::valueChanged,
        this, &QuickBrightnessWindow::onSliderValueChanged);
}

QuickBrightnessWindow::~QuickBrightnessWindow()
{
    applyPendingBrightness(); // no perder el último valor si se destruye con timer activo
    delete ui;
}

void QuickBrightnessWindow::setupIcon()
{
    const QIcon icon = QIcon::fromTheme(QStringLiteral("display-brightness-symbolic"));
    if (!icon.isNull()) {
        ui->iconLabel->setPixmap(icon.pixmap(32, 32));
    }
    else {
        // Windows (o tema sin ícono): glifo de respaldo.
        ui->iconLabel->setText(QStringLiteral("\u2600"));
    }
}

void QuickBrightnessWindow::loadCurrentBrightness()
{
    const bool available = m_brightness && m_brightness->isAvailable();
    ui->brightnessSlider->setEnabled(available);

    // Si no se puede leer, 100 como en el script original.
    const int current = available ? m_brightness->brightnessPercent().value_or(100) : 100;
    const int snapped = qBound(ui->brightnessSlider->minimum(), snapToStep(current),
        ui->brightnessSlider->maximum());

    const QSignalBlocker blocker(ui->brightnessSlider);
    ui->brightnessSlider->setValue(snapped);
    m_lastApplied = snapped;
    m_pendingValue = snapped;
}

int QuickBrightnessWindow::snapToStep(int value)
{
    return qRound(value / static_cast<double>(kStep)) * kStep;
}

void QuickBrightnessWindow::onSliderValueChanged(int value)
{
    // Equivale al paso de 5 de Gtk.Scale.
    const int snapped = snapToStep(value);
    if (snapped != value) {
        const QSignalBlocker blocker(ui->brightnessSlider);
        ui->brightnessSlider->setValue(snapped);
    }

    // Debounce: cada cambio reinicia el timer; solo se aplica el valor final.
    m_pendingValue = snapped;
    m_applyTimer->start();
}

void QuickBrightnessWindow::applyPendingBrightness()
{
    m_applyTimer->stop();

    if (!m_brightness || m_pendingValue < 0 || m_pendingValue == m_lastApplied)
        return;

    m_lastApplied = m_pendingValue;
    m_brightness->setBrightnessPercent(m_pendingValue);
}

void QuickBrightnessWindow::moveToBottomRight()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect area = screen->availableGeometry();
    move(area.x() + area.width() - width() - kMarginRight,
        area.y() + area.height() - height() - kMarginBottom);
}

void QuickBrightnessWindow::showEvent(QShowEvent* event)
{
    moveToBottomRight();
    QWidget::showEvent(event);
}

void QuickBrightnessWindow::hideEvent(QHideEvent* event)
{
    // Si el popup se cierra antes de que dispare el timer, aplicar el valor pendiente.
    applyPendingBrightness();
    QWidget::hideEvent(event);
}

void QuickBrightnessWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}