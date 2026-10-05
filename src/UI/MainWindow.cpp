#include "MainWindow.h"
#include "ui_mainwindow.h"
#include <QScreen>
#include <QGuiApplication>

MainWindow::MainWindow(std::shared_ptr<ISystemControl> systemControl, std::shared_ptr<ISystemKey> systemKey,
    std::shared_ptr<ISystemBattery> systemBattery, std::shared_ptr<ISystemBluetoothControl> systemBluetooth,
    std::shared_ptr<ISystemDisplayBrightness> systemDisplayBrightness,
    std::shared_ptr<ISystemDateTimeSettingsLauncher> systemDateTimeSettings,
    QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_systemControl(std::move(systemControl))
    , m_systemKey(std::move(systemKey))
    , m_systemBattery(std::move(systemBattery))
    , m_systemBluetooth(std::move(systemBluetooth))
    , m_systemDisplayBrightness(std::move(systemDisplayBrightness))
    , m_systemDateTimeSettings(std::move(systemDateTimeSettings))
{
    ui->setupUi(this);

    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    this->setAttribute(Qt::WA_TranslucentBackground);

    ui->centralwidget->setAttribute(Qt::WA_StyledBackground, true);
    ui->centralwidget->setStyleSheet("QWidget#centralwidget { background-color: #fcfcfc; border-radius: 90px; }");

    this->setupPanelGeometry(this, 0.95, 0.05);

    m_compatWindow = std::make_unique<CompatWindow>(m_systemControl, this);
    m_keySequenceWindow = std::make_unique<KeySequenceWindow>(m_systemKey, this);
    m_QuickBatteryWindow = std::make_unique<QuickBatteryWindow>(m_systemBattery, this);
    m_QuickBluetoothWindow = std::make_unique<QuickBluetoothWindow>(m_systemBluetooth, this);
    m_QuickBrightnessWindow = std::make_unique<QuickBrightnessWindow>(m_systemDisplayBrightness, this);
    m_ClockCalendarPopupWindow = std::make_unique<ClockCalendarPopupWindow>(m_systemDateTimeSettings, this);

    // Conexiones de botones a slots
    connect(ui->btnBattery, &QPushButton::clicked, this, &MainWindow::onBatteryClicked);
    connect(ui->btnBluetooth, &QPushButton::clicked, this, &MainWindow::onBluetoothClicked);
    connect(ui->btnBrightness, &QPushButton::clicked, this, &MainWindow::onBrightnessClicked);
    connect(ui->btnClock, &QPushButton::clicked, this, &MainWindow::onClockClicked);

    // Timer de ocultación de batería
    m_batteryHideTimer = new QTimer(this);
    m_batteryHideTimer->setSingleShot(true);
    connect(m_batteryHideTimer, &QTimer::timeout, this, [this]() {
        if (m_QuickBatteryWindow) {
            m_QuickBatteryWindow->hide();
        }
        });

    // Timer de actualización de la batería
    m_batteryTimer = new QTimer(this);
    connect(m_batteryTimer, &QTimer::timeout, this, &MainWindow::updateBatteryStatus);
    m_batteryTimer->start(5000);

    // Timer de actualización de reloj del botón (cada 1s para precisión inmediata)
    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &MainWindow::updateClockButton);
    m_clockTimer->start(1000);

    updateBatteryStatus();
    updateClockButton();
    updateBatteryWindowPosition();
    updateBrightnessWindowPosition();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::updateClockButton() {
    if (!ui->btnClock) return;

    const QString timeStr = ClockCalendarPopupWindow::getFormattedTime12h(/*includeSeconds=*/false);
    const QString dateStr = ClockCalendarPopupWindow::getFormattedDate();

    ui->btnClock->setText(QString("%1\n%2").arg(timeStr, dateStr));
}

void MainWindow::onClockClicked() {
    if (!m_ClockCalendarPopupWindow) return;

    if (m_ClockCalendarPopupWindow->isVisible()) {
        m_ClockCalendarPopupWindow->hide();
    }
    else {
        updateClockWindowPosition();
        m_ClockCalendarPopupWindow->show();
        m_ClockCalendarPopupWindow->activateWindow();
    }
}

void MainWindow::updateClockWindowPosition() {
    if (!m_ClockCalendarPopupWindow || !ui->btnClock) return;

    m_ClockCalendarPopupWindow->adjustSize();

    // 1. Calcular posX (centrado con respecto al botón de reloj)
    QPoint globalBtnPos = ui->btnClock->mapToGlobal(QPoint(0, 0));
    int btnCenterX = globalBtnPos.x() + (ui->btnClock->width() / 2);
    int posX = btnCenterX - (m_ClockCalendarPopupWindow->width() / 2);

    // 2. Calcular posY (pegado exactamente al borde superior del panel)
    QPoint globalPanelPos = this->mapToGlobal(QPoint(0, 0));
    int posY = globalPanelPos.y() - m_ClockCalendarPopupWindow->height();

    // 3. Asegurar que la ventana no se corte en los bordes de la pantalla
    QScreen* screen = this->screen();
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeo = screen->availableGeometry();
        if (posX + m_ClockCalendarPopupWindow->width() > screenGeo.right()) {
            posX = screenGeo.right() - m_ClockCalendarPopupWindow->width();
        }
        if (posX < screenGeo.left()) {
            posX = screenGeo.left();
        }
    }

    m_ClockCalendarPopupWindow->move(posX, posY);
}

void MainWindow::onBluetoothClicked() {
    if (!m_QuickBluetoothWindow) return;

    if (m_QuickBluetoothWindow->isVisible()) {
        m_QuickBluetoothWindow->hide();
    }
    else {
        updateBluetoothWindowPosition();
        m_QuickBluetoothWindow->show();
        m_QuickBluetoothWindow->activateWindow();
    }
}

void MainWindow::onBrightnessClicked() {
    if (!m_QuickBrightnessWindow) return;

    if (m_QuickBrightnessWindow->isVisible()) {
        m_QuickBrightnessWindow->hide();
    }
    else {
        updateBrightnessWindowPosition();
        m_QuickBrightnessWindow->show();
        m_QuickBrightnessWindow->activateWindow();
    }
}

void MainWindow::updateBluetoothWindowPosition() {
    if (!m_QuickBluetoothWindow || !ui->btnBluetooth) return;

    m_QuickBluetoothWindow->adjustSize();

    QPoint globalBtnPos = ui->btnBluetooth->mapToGlobal(QPoint(0, 0));
    int btnCenterX = globalBtnPos.x() + (ui->btnBluetooth->width() / 2);
    int posX = btnCenterX - (m_QuickBluetoothWindow->width() / 2);
    int posY = globalBtnPos.y() - m_QuickBluetoothWindow->height() - 8;

    m_QuickBluetoothWindow->move(posX, posY);
}

void MainWindow::onBatteryClicked() {
    if (!m_QuickBatteryWindow) return;

    if (m_QuickBatteryWindow->isVisible()) {
        m_QuickBatteryWindow->hide();
    }
    else {
        updateBatteryWindowPosition();
        m_QuickBatteryWindow->show();
        m_QuickBatteryWindow->activateWindow();
    }
}

void MainWindow::updateBatteryWindowPosition() {
    if (!m_QuickBatteryWindow || !ui->btnBattery) return;

    m_QuickBatteryWindow->adjustSize();

    QPoint globalBtnPos = ui->btnBattery->mapToGlobal(QPoint(0, 0));
    int btnCenterX = globalBtnPos.x() + (ui->btnBattery->width() / 2);
    int posX = btnCenterX - (m_QuickBatteryWindow->width() / 2);
    int posY = globalBtnPos.y() - m_QuickBatteryWindow->height() - 8;

    m_QuickBatteryWindow->move(posX, posY);
}

void MainWindow::updateBrightnessWindowPosition() {
    if (!m_QuickBrightnessWindow || !ui->btnBrightness) return;

    m_QuickBrightnessWindow->adjustSize();

    QPoint globalBtnPos = ui->btnBrightness->mapToGlobal(QPoint(0, 0));
    int btnCenterX = globalBtnPos.x() + (ui->btnBrightness->width() / 2);
    int posX = btnCenterX - (m_QuickBrightnessWindow->width() / 2);
    int posY = globalBtnPos.y() - m_QuickBrightnessWindow->height() - 8;

    m_QuickBrightnessWindow->move(posX, posY);
}

void MainWindow::updateBatteryStatus() {
    if (!m_systemBattery) return;

    BatteryInfo info = m_systemBattery->readBattery();

    if (!info.present) {
        ui->btnBattery->setText(QStringLiteral("--%"));
        ui->btnBattery->setToolTip(tr("Batería no detectada"));
        return;
    }

    ui->btnBattery->setText(QString("%1%").arg(info.capacityPercent));

    QString timeStr = tr("Calculando...");
    if (info.remainingMinutes) {
        int hours = *info.remainingMinutes / 60;
        int mins = *info.remainingMinutes % 60;
        timeStr = QString("%1h %2m").arg(hours).arg(mins);
    }

    QString powerStr = info.powerWatts
        ? QString("%1 W").arg(*info.powerWatts, 0, 'f', 1)
        : tr("N/D");

    QString toolTipText = QString(
        "<b>Batería:</b> %1%<br>"
        "<b>Tiempo restante:</b> %2<br>"
        "<b>Consumo actual:</b> %3"
    ).arg(info.capacityPercent).arg(timeStr).arg(powerStr);

    ui->btnBattery->setToolTip(toolTipText);
}

void MainWindow::setupPanelGeometry(QWidget* widget, double widthRatio, double heightRatio) {
    if (!widget) return;

    QScreen* screen = widget->screen();
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) return;

    QRect screenGeo = screen->geometry();

    int panelWidth = static_cast<int>(screenGeo.width() * widthRatio);
    int panelHeight = static_cast<int>(screenGeo.height() * heightRatio);

    int posX = (screenGeo.width() - panelWidth) / 2;
    int posY = screenGeo.height() - panelHeight;

    posX += screenGeo.x();
    posY += screenGeo.y();

    widget->setGeometry(posX, posY, panelWidth, panelHeight);
}