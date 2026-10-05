#include "QuickBluetoothWindow.h"
#include "ui_quickbluetoothwindow.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QRunnable>
#include <QScreen>
#include <QThreadPool>
#include <QTimer>

bool waitForConnectionState(const std::shared_ptr<ISystemBluetoothControl>& control,
    const QString& mac, bool desiredConnected,
    int timeoutMs, int pollMs = 250) {
    QElapsedTimer timer;
    timer.start();
    do {
        const auto devices = control->knownDevices();
        for (const auto& dev : devices) {
            if (dev.mac.compare(mac, Qt::CaseInsensitive) == 0) {
                if (dev.connected == desiredConnected)
                    return true;
                break;
            }
        }
        QThread::msleep(pollMs);
    } while (timer.elapsed() < timeoutMs);
    return false;
}

void QuickBluetoothWindow::onDeviceActionFinished(const QString& mac, bool connectAction, ActionResult result) {
    m_pendingMacs.remove(mac);
    if (result == ActionResult::Failed) {
        m_rowMessages.insert(mac, connectAction ? tr("No se pudo conectar")
            : tr("No se pudo desconectar"));
    }
    refreshAsync();
}

QuickBluetoothWindow::QuickBluetoothWindow(std::shared_ptr<ISystemBluetoothControl> control, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::QuickBluetoothWindow)
    , m_control(std::move(control)) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    ui->setupUi(this);

    const QIcon btIcon = QIcon::fromTheme("bluetooth-active-symbolic");
    if (btIcon.isNull())
        ui->iconLabel->hide();
    else
        ui->iconLabel->setPixmap(btIcon.pixmap(24, 24));

    const QIcon settingsIcon = QIcon::fromTheme("emblem-system-symbolic");
    if (settingsIcon.isNull())
        ui->settingsButton->setText(QString::fromUtf8("\xE2\x9A\x99"));
    else
        ui->settingsButton->setIcon(settingsIcon);

    connect(ui->powerSwitch, &QCheckBox::toggled, this, &QuickBluetoothWindow::onPowerToggled);
    connect(ui->settingsButton, &QToolButton::clicked, this, &QuickBluetoothWindow::onSettingsClicked);

    refreshAsync();
}

QuickBluetoothWindow::~QuickBluetoothWindow() {
    delete ui;
}

void QuickBluetoothWindow::showAtBottomRight() {
    adjustSize();
    const QRect geometry = QGuiApplication::primaryScreen()->geometry();
    const int x = geometry.x() + geometry.width() - width() - 200;
    const int y = geometry.y() + geometry.height() - height() - 60;
    move(x, y);
    show();
    activateWindow();
}

void QuickBluetoothWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}

void QuickBluetoothWindow::closeEvent(QCloseEvent* event) {
    QWidget::closeEvent(event);
    //QCoreApplication::quit();
}

void QuickBluetoothWindow::refreshAsync() {
    QPointer<QuickBluetoothWindow> self(this);
    auto control = m_control;

    QThreadPool::globalInstance()->start(QRunnable::create([self, control]() {
        Snapshot snapshot;
        snapshot.powered = control->isPowered();
        snapshot.devices = control->knownDevices();

        if (!self)
            return;
        QMetaObject::invokeMethod(self.data(), [self, snapshot]() {
            if (self)
                self->applySnapshot(snapshot);
            }, Qt::QueuedConnection);
        }));
}

void QuickBluetoothWindow::applySnapshot(const Snapshot& snapshot) {
    m_ignorePowerSignal = true;
    ui->powerSwitch->setChecked(snapshot.powered);
    m_ignorePowerSignal = false;

    clearDeviceList();
    for (const BluetoothDeviceInfo& device : snapshot.devices)
        addDeviceRow(device);
    ui->deviceListLayout->addStretch(1);
}

void QuickBluetoothWindow::clearDeviceList() {
    while (QLayoutItem* item = ui->deviceListLayout->takeAt(0)) {
        if (QWidget* widget = item->widget())
            widget->deleteLater();
        delete item;
    }
}

void QuickBluetoothWindow::addDeviceRow(const BluetoothDeviceInfo& device) {
    auto* row = new QWidget(ui->scrollAreaWidgetContents);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);

    auto* label = new QLabel(row);
    label->setTextFormat(Qt::RichText);
    const QString safeName = device.name.toHtmlEscaped();
    QString text = device.connected ? QString("<b>%1</b> (Conectado)").arg(safeName) : safeName;
    if (m_rowMessages.contains(device.mac))
        text += QString(" <span style='color:#d9534f'>– %1</span>").arg(m_rowMessages.value(device.mac).toHtmlEscaped());
    label->setText(text);

    const bool pending = m_pendingMacs.contains(device.mac);
    auto* button = new QPushButton(pending ? tr("...") : (device.connected ? tr("Desconectar") : tr("Conectar")), row);
    button->setEnabled(!pending);

    const QString mac = device.mac;
    const bool connectAction = !device.connected;
    connect(button, &QPushButton::clicked, this, [this, button, mac, connectAction]() {
        runDeviceAction(button, mac, connectAction);
        });

    rowLayout->addWidget(label, 1);
    rowLayout->addWidget(button, 0);
    ui->deviceListLayout->addWidget(row);
}

#include <QThread>

void QuickBluetoothWindow::runDeviceAction(QPushButton* button, const QString& mac, bool connectAction) {
    if (m_pendingMacs.contains(mac))
        return; // ya hay una acción en curso para este dispositivo

    m_pendingMacs.insert(mac);
    m_rowMessages.remove(mac);
    button->setText(connectAction ? tr("Conectando...") : tr("Desconectando..."));
    button->setEnabled(false);

    QPointer<QuickBluetoothWindow> self(this);
    auto control = m_control;

    QThreadPool::globalInstance()->start(QRunnable::create([self, control, mac, connectAction]() {
        const bool requested = connectAction ? control->connectDevice(mac)
            : control->disconnectDevice(mac);

        // Siempre esperamos al estado real del SO. Si la petición ni siquiera fue
        // aceptada, solo damos un margen corto; si fue aceptada, damos tiempo completo.
        const int timeoutMs = requested ? (connectAction ? 20000 : 8000) : 3000;
        const bool reached = waitForConnectionState(control, mac, connectAction, timeoutMs);

        if (!self)
            return;

        const ActionResult result = reached ? ActionResult::Success : ActionResult::Failed;
        QMetaObject::invokeMethod(self.data(), [self, mac, connectAction, result]() {
            if (self)
                self->onDeviceActionFinished(mac, connectAction, result);
            }, Qt::QueuedConnection);
        }));
}

void QuickBluetoothWindow::onPowerToggled(bool enabled) {
    if (m_ignorePowerSignal)
        return;

    QPointer<QuickBluetoothWindow> self(this);
    auto control = m_control;

    QThreadPool::globalInstance()->start(QRunnable::create([control, enabled]() {
        control->setPowered(enabled);
        }));

    QTimer::singleShot(1000, this, [this]() { refreshAsync(); });
}

void QuickBluetoothWindow::onSettingsClicked() {
    m_control->openBluetoothSettings();
    close();
}