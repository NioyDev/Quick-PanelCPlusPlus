#include "MainWindow.h"
#include "ui_mainwindow.h"
#include <QScreen>

MainWindow::MainWindow(std::shared_ptr<ISystemControl> systemControl, std::shared_ptr<ISystemKey> systemKey,
    std::shared_ptr<ISystemBattery> systemBattery,
    QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_systemControl(std::move(systemControl))
    , m_systemKey(std::move(systemKey))
    , m_systemBattery(std::move(systemBattery)) {
    ui->setupUi(this);

    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    this->setAttribute(Qt::WA_TranslucentBackground);

    ui->centralwidget->setAttribute(Qt::WA_StyledBackground, true);
    ui->centralwidget->setStyleSheet("QWidget#centralwidget { background-color: #fcfcfc; border-radius: 90px; }");

    this->setupPanelGeometry(this, 0.95, 0.05);

    m_compatWindow = std::make_unique<CompatWindow>(m_systemControl, this);
    m_keySequenceWindow = std::make_unique<KeySequenceWindow>(m_systemKey, this);
    m_QuickBatteryWindow = std::make_unique<QuickBatteryWindow>(m_systemBattery, this);

    // Abrir/cerrar ventana emergente al hacer clic en el botón de batería
    connect(ui->btnBattery, &QPushButton::clicked, this, &MainWindow::onBatteryClicked);

    // Timer de 1 segundo para ocultar QuickBatteryWindow cuando el cursor sale de ella
    m_batteryHideTimer = new QTimer(this);
    m_batteryHideTimer->setSingleShot(true);
    connect(m_batteryHideTimer, &QTimer::timeout, this, [this]() {
        if (m_QuickBatteryWindow) {
            m_QuickBatteryWindow->hide();
        }
        });

    // Cancelar cuenta regresiva si el cursor entra a QuickBatteryWindow
    connect(m_QuickBatteryWindow.get(), &QuickBatteryWindow::mouseEnteredWindow, this, [this]() {
        m_batteryHideTimer->stop();
        });

    // Iniciar cuenta regresiva al salir el cursor de QuickBatteryWindow
    connect(m_QuickBatteryWindow.get(), &QuickBatteryWindow::mouseLeftWindow, this, [this]() {
        m_batteryHideTimer->start(1000);
        });

    // Timer de actualización de la batería
    m_batteryTimer = new QTimer(this);
    connect(m_batteryTimer, &QTimer::timeout, this, &MainWindow::updateBatteryStatus);
    m_batteryTimer->start(5000);

    updateBatteryStatus();
    updateBatteryWindowPosition();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::onBatteryClicked() {
    if (!m_QuickBatteryWindow) return;

    if (m_QuickBatteryWindow->isVisible()) {
        m_QuickBatteryWindow->hide();
    }
    else {
        updateBatteryWindowPosition();
        m_QuickBatteryWindow->show();
        // Tiempo inicial de tolerancia para deslizar el ratón hacia la ventana
        m_batteryHideTimer->start(2000);
    }
}

void MainWindow::updateBatteryWindowPosition() {
    if (!m_QuickBatteryWindow || !ui->btnBattery) return;

    m_QuickBatteryWindow->adjustSize();

    // Obtener la posición global del botón de la batería
    QPoint globalBtnPos = ui->btnBattery->mapToGlobal(QPoint(0, 0));

    // Centro X del botón
    int btnCenterX = globalBtnPos.x() + (ui->btnBattery->width() / 2);

    // Centrar la ventana respecto al centro del botón en X
    int posX = btnCenterX - (m_QuickBatteryWindow->width() / 2);

    // Posición Y fija justo encima del botón
    int posY = globalBtnPos.y() - m_QuickBatteryWindow->height() - 8;

    m_QuickBatteryWindow->move(posX, posY);
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