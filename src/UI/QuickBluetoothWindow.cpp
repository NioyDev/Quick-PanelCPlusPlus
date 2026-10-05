#include "QuickBluetoothWindow.h"
#include "ui_quickbluetoothwindow.h"

#include <QCoreApplication>
#include <QElapsedTimer>
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
#include <QThread>
#include <QThreadPool>
#include <QTimer>

namespace {

    // Bloquea (solo llamar desde un hilo de trabajo) hasta que el SO refleje el estado deseado.
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

    bool sameSnapshot(bool powerA, const QList<BluetoothDeviceInfo>& a,
        bool powerB, const QList<BluetoothDeviceInfo>& b) {
        if (powerA != powerB || a.size() != b.size())
            return false;
        for (int i = 0; i < a.size(); ++i) {
            if (a[i].mac != b[i].mac || a[i].name != b[i].name || a[i].connected != b[i].connected)
                return false;
        }
        return true;
    }

} // namespace

QuickBluetoothWindow::QuickBluetoothWindow(std::shared_ptr<ISystemBluetoothControl> control, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::QuickBluetoothWindow)
    , m_control(std::move(control)) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    ui->setupUi(this);

    m_restartButton = new QPushButton(tr("Reiniciar adaptador Bluetooth"), ui->bluetoothBox);
    m_restartButton->setObjectName("deviceActionButton");   // reutiliza tu estilo
    m_restartButton->setCursor(Qt::PointingHandCursor);
    ui->boxLayout->addWidget(m_restartButton);
    connect(m_restartButton, &QPushButton::clicked, this, &QuickBluetoothWindow::onRestartAdapterClicked);

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

    connect(ui->powerSwitch, &QAbstractButton::toggled, this, &QuickBluetoothWindow::onPowerToggled);
    connect(ui->settingsButton, &QToolButton::clicked, this, &QuickBluetoothWindow::onSettingsClicked);

    // Sondeo ligero: solo corre mientras la ventana está visible (ver showEvent/hideEvent)
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(2000);
    connect(m_pollTimer, &QTimer::timeout, this, [this]() {
        if (!m_powerChangePending)
            refreshAsync(false);
        });

    // El primer refresco lo hace showEvent()
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

void QuickBluetoothWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    m_rowMessages.clear();
    refreshAsync(true);     // estado fresco cada vez que la ventana aparece
    m_pollTimer->start();
}

void QuickBluetoothWindow::hideEvent(QHideEvent* event) {
    m_pollTimer->stop();
    QWidget::hideEvent(event);
}

void QuickBluetoothWindow::refreshAsync(bool force) {
    if (m_refreshInFlight) {            // ya hay uno en curso: encolar otro al terminar
        m_refreshQueued = true;
        m_queuedForce = m_queuedForce || force;
        return;
    }
    m_refreshInFlight = true;

    QPointer<QuickBluetoothWindow> self(this);
    auto control = m_control;

    QThreadPool::globalInstance()->start(QRunnable::create([self, control, force]() {
        Snapshot snapshot;
        snapshot.powered = control->isPowered();
        snapshot.devices = control->knownDevices();

        if (!self)
            return;
        QMetaObject::invokeMethod(self.data(), [self, snapshot, force]() {
            if (self)
                self->onSnapshotReady(snapshot, force);
            }, Qt::QueuedConnection);
        }));
}

void QuickBluetoothWindow::onSnapshotReady(const Snapshot& snapshot, bool force) {
    m_refreshInFlight = false;

    // Solo reconstruimos la UI si algo cambió (evita parpadeos con el sondeo)
    if (force || !m_hasSnapshot ||
        !sameSnapshot(snapshot.powered, snapshot.devices,
            m_lastSnapshot.powered, m_lastSnapshot.devices)) {
        m_lastSnapshot = snapshot;
        m_hasSnapshot = true;
        applySnapshot(snapshot);
    }

    if (m_refreshQueued) {
        const bool queuedForce = m_queuedForce;
        m_refreshQueued = false;
        m_queuedForce = false;
        refreshAsync(queuedForce);
    }
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
    if (m_rowMessages.contains(device.mac)) {
        text += QString(" <span style='color:#d9534f'>– %1</span>")
            .arg(m_rowMessages.value(device.mac).toHtmlEscaped());
    }
    label->setText(text);

    // Si hay una acción en curso para este dispositivo, el botón se mantiene deshabilitado
    const bool pending = m_pendingMacs.contains(device.mac);
    auto* button = new QPushButton(
        pending ? tr("...") : (device.connected ? tr("Desconectar") : tr("Conectar")), row);
    button->setObjectName("deviceActionButton");
    button->setCursor(Qt::PointingHandCursor);
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

void QuickBluetoothWindow::onDeviceActionFinished(const QString& mac, bool connectAction, ActionResult result) {
    m_pendingMacs.remove(mac);
    if (result == ActionResult::Failed) {
        m_rowMessages.insert(mac, connectAction ? tr("No se pudo conectar")
            : tr("No se pudo desconectar"));
    }
    refreshAsync(true);   // force: para que se vea el mensaje aunque el estado no cambie
}

void QuickBluetoothWindow::onPowerToggled(bool enabled) {
    if (m_ignorePowerSignal)
        return;

    m_powerChangePending = true;   // evita que el sondeo revierta el switch con un estado viejo

    QPointer<QuickBluetoothWindow> self(this);
    auto control = m_control;

    QThreadPool::globalInstance()->start(QRunnable::create([self, control, enabled]() {
        control->setPowered(enabled);
        QThread::msleep(300);   // pequeño margen para que el SO refleje el estado

        if (!self)
            return;
        QMetaObject::invokeMethod(self.data(), [self]() {
            if (!self)
                return;
            self->m_powerChangePending = false;
            self->refreshAsync(true);
            }, Qt::QueuedConnection);
        }));
}

void QuickBluetoothWindow::onSettingsClicked() {
    m_control->openBluetoothSettings();
    close();
}

void QuickBluetoothWindow::onRestartAdapterClicked() {
    m_restartButton->setEnabled(false);
    m_restartButton->setText(tr("Reiniciando..."));
    m_powerChangePending = true;     // pausa el sondeo mientras el adaptador no está

    QPointer<QuickBluetoothWindow> self(this);
    auto control = m_control;

    QThreadPool::globalInstance()->start(QRunnable::create([self, control]() {
        control->restartAdapter();
        QThread::msleep(2000);       // dejar que el adaptador se reinicialice

        if (!self)
            return;
        QMetaObject::invokeMethod(self.data(), [self]() {
            if (!self)
                return;
            self->m_powerChangePending = false;
            self->m_restartButton->setEnabled(true);
            self->m_restartButton->setText(self->tr("Reiniciar adaptador Bluetooth"));
            self->refreshAsync(true);
            }, Qt::QueuedConnection);
        }));
}