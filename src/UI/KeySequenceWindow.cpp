#include "KeySequenceWindow.h"
#include "ui_keysequencewindow.h"
#include <QtConcurrent>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

KeySequenceWindow::KeySequenceWindow(std::shared_ptr<ISystemKey> systemKey, QWidget* parent)
    : QMainWindow(parent), ui(new Ui::KeySequenceWindow), m_systemKey(std::move(systemKey)) {
    ui->setupUi(this);

    // Mantener la ventana siempre arriba en la jerarquía de z-index de Windows
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

    // Inicializar temporizadores para debounce
    m_delayTimer = new QTimer(this);
    m_delayTimer->setSingleShot(true);
    m_delayTimer->setInterval(250);

    m_rateTimer = new QTimer(this);
    m_rateTimer->setSingleShot(true);
    m_rateTimer->setInterval(250);

    ui->entryKeys->installEventFilter(this);

    setupConnections();
    applyStyleSheet();
    loadDataAsync();
}

KeySequenceWindow::~KeySequenceWindow() {
    delete ui;
}

void KeySequenceWindow::toggleVisibility() {
    if (isVisible()) {
        hide();
    }
    else {
        showNormal();
        activateWindow();
        raise();
    }
}

void KeySequenceWindow::setupConnections() {
    connect(m_systemKey.get(), &ISystemKey::toggleRequested, this, &KeySequenceWindow::toggleVisibility);

    connect(ui->btnAdd, &QPushButton::clicked, this, &KeySequenceWindow::onAddClicked);

    connect(ui->switchRepeat, &QCheckBox::toggled, [this](bool checked) {
        if (!m_isLoading) m_systemKey->setKeyRepeatEnabled(checked);
        });

    connect(m_delayTimer, &QTimer::timeout, this, [this]() {
        if (!m_isLoading) m_systemKey->setKeyRepeatDelay(ui->scaleDelay->value());
        });

    connect(m_rateTimer, &QTimer::timeout, this, [this]() {
        if (!m_isLoading) m_systemKey->setKeyRepeatRate(ui->scaleRate->value());
        });

    connect(ui->scaleDelay, &QSlider::valueChanged, this, [this](int val) {
        ui->lblDelayValue->setText(QString("%1 ms").arg(val));
        if (!m_isLoading) {
            m_delayTimer->start();
        }
        });

    connect(ui->scaleRate, &QSlider::valueChanged, this, [this](int val) {
        ui->lblRateValue->setText(QString("%1 char/s").arg(val));
        if (!m_isLoading) {
            m_rateTimer->start();
        }
        });
}

bool KeySequenceWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == ui->entryKeys && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        int key = keyEvent->key();

        if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta) {
            return true;
        }

        if (key == Qt::Key_Backspace && keyEvent->modifiers() == Qt::NoModifier) {
            m_currentRawKey.clear();
            ui->entryKeys->clear();
            return true;
        }

        QStringList mods;
        QStringList humanMods;

        if (keyEvent->modifiers() & Qt::ControlModifier) { mods << "<Primary>"; humanMods << "Ctrl + "; }
        if (keyEvent->modifiers() & Qt::AltModifier) { mods << "<Alt>";     humanMods << "Alt + "; }
        if (keyEvent->modifiers() & Qt::ShiftModifier) { mods << "<Shift>";   humanMods << "Shift + "; }
        if (keyEvent->modifiers() & Qt::MetaModifier) { mods << "<Super>";   humanMods << "Super + "; }

        QString keyText = QKeySequence(key).toString(QKeySequence::PortableText).toLower();
        m_currentRawKey = mods.join("") + keyText;

        QString humanText = humanMods.join("") + QKeySequence(key).toString(QKeySequence::NativeText);
        ui->entryKeys->setText(humanText);
        ui->entryCmd->setFocus();
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

void KeySequenceWindow::loadDataAsync() {
    m_isLoading = true;
    (void)QtConcurrent::run([this]() {
        bool repeat = m_systemKey->getKeyRepeatEnabled();
        int delay = m_systemKey->getKeyRepeatDelay();
        int rate = m_systemKey->getKeyRepeatRate();
        auto shortcuts = m_systemKey->loadShortcuts();

        QMetaObject::invokeMethod(this, [=]() {
            ui->switchRepeat->setChecked(repeat);

            ui->scaleDelay->setValue(delay);
            ui->lblDelayValue->setText(QString("%1 ms").arg(delay));

            ui->scaleRate->setValue(rate);
            ui->lblRateValue->setText(QString("%1 char/s").arg(rate));

            ui->listWidget->clear();
            for (const auto& item : shortcuts) {
                renderShortcutItem(item);
            }
            m_isLoading = false;
            });
        });
}

void KeySequenceWindow::renderShortcutItem(const ShortcutItem& item) {
    QListWidgetItem* listItem = new QListWidgetItem(ui->listWidget);
    QWidget* rowWidget = new QWidget();
    rowWidget->setObjectName("cardRow");

    QHBoxLayout* layout = new QHBoxLayout(rowWidget);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(20);

    QLabel* lblKey = new QLabel(item.displayKey);
    lblKey->setStyleSheet("font-size: 14px; font-weight: bold; color: #ffffff; min-width: 140px;");

    QVBoxLayout* vbox = new QVBoxLayout();
    QLabel* lblDesc = new QLabel(item.description.isEmpty() ? "Atajo del Sistema" : item.description);
    lblDesc->setStyleSheet("font-size: 13px; font-weight: 600; color: #e5e5ea;");

    QLabel* lblCmd = new QLabel(item.command);
    lblCmd->setStyleSheet("font-family: monospace; font-size: 11px; color: #8e8e93;");

    vbox->addWidget(lblDesc);
    vbox->addWidget(lblCmd);

    QPushButton* btnDel = new QPushButton("✕");
    btnDel->setObjectName("btnDelete");
    connect(btnDel, &QPushButton::clicked, [this, rawKey = item.rawKey, listItem]() {
        onDeleteClicked(rawKey, listItem);
        });

    layout->addWidget(lblKey);
    layout->addLayout(vbox, 1);
    layout->addWidget(btnDel);

    listItem->setSizeHint(rowWidget->sizeHint());
    ui->listWidget->setItemWidget(listItem, rowWidget);
}

void KeySequenceWindow::onAddClicked() {
    QString rawKey = m_currentRawKey;
    QString cmd = ui->entryCmd->text().trimmed();
    QString desc = ui->entryDesc->text().trimmed();

    if (rawKey.isEmpty() || cmd.isEmpty()) return;

    // Bloquear si intenta registrar el atajo reservado del sistema
    if (rawKey.toLower() == "<primary>k") {
        QMessageBox::warning(this,
            "Atajo Reservado",
            "La combinación 'Ctrl + K' está reservada exclusivamente para alternar la visibilidad de este panel.");
        ui->entryKeys->clear();
        m_currentRawKey.clear();
        ui->entryKeys->setFocus();
        return;
    }

    ShortcutItem item{ ui->entryKeys->text(), rawKey, cmd, desc };
    if (m_systemKey->addShortcut(item)) {
        renderShortcutItem(item);
        ui->entryKeys->clear();
        ui->entryCmd->clear();
        ui->entryDesc->clear();
        m_currentRawKey.clear();
        ui->entryKeys->setFocus();
    }
}

void KeySequenceWindow::onDeleteClicked(const QString& rawKey, QListWidgetItem* item) {
    if (m_systemKey->removeShortcut(rawKey)) {
        delete ui->listWidget->takeItem(ui->listWidget->row(item));
    }
}

void KeySequenceWindow::applyStyleSheet() {
    QString qss = R"(
        QMainWindow, QWidget {
            background-color: #09090b;
            color: #fafafa;
            font-family: 'Segoe UI', system-ui, sans-serif;
        }
        QTabWidget::pane {
            border: none;
            background: #09090b;
        }
        QTabBar::tab {
            background-color: transparent;
            color: #a1a1aa;
            padding: 8px 18px;
            border-radius: 6px;
            font-weight: 500;
            margin-right: 4px;
        }
        QTabBar::tab:hover {
            background-color: #18181b;
            color: #fafafa;
        }
        QTabBar::tab:selected {
            background-color: #27272a;
            color: #fafafa;
        }
        QLineEdit {
            background-color: #09090b;
            color: #fafafa;
            border-radius: 8px;
            border: 1px solid #27272a;
            padding: 8px 12px;
        }
        QLineEdit:focus {
            border: 1px solid #71717a;
        }
        QPushButton#btnAdd {
            background-color: #fafafa;
            color: #09090b;
            border-radius: 8px;
            font-weight: bold;
            padding: 8px 20px;
            border: none;
        }
        QPushButton#btnAdd:hover {
            background-color: #e4e4e7;
        }
        QPushButton#btnDelete {
            background-color: transparent;
            color: #71717a;
            border-radius: 6px;
            font-size: 14px;
            font-weight: bold;
            padding: 4px 8px;
            border: none;
        }
        QPushButton#btnDelete:hover {
            background-color: rgba(239, 68, 68, 0.15);
            color: #ef4444;
        }
        QListWidget {
            background: transparent;
            border: none;
            outline: none;
        }
        QWidget#cardRow {
            background-color: #18181b;
            border: 1px solid #27272a;
            border-radius: 8px;
        }
        QWidget#cardRow:hover {
            background-color: #27272a;
            border: 1px solid #3f3f46;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: #27272a;
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: #fafafa;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #fafafa;
            width: 16px;
            height: 16px;
            margin: -5px 0;
            border-radius: 8px;
        }
        QCheckBox::indicator {
            width: 20px;
            height: 20px;
            border-radius: 5px;
            border: 1px solid #3f3f46;
            background-color: #18181b;
        }
        QCheckBox::indicator:hover {
            border-color: #71717a;
        }
        QCheckBox::indicator:checked {
            background-color: #fafafa;
            border-color: #fafafa;
            image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='%2309090b' stroke-width='3.5' stroke-linecap='round' stroke-linejoin='round'><polyline points='20 6 9 17 4 12'></polyline></svg>");
        }
        QMessageBox {
            background-color: #18181b;
            color: #fafafa;
        }
        QMessageBox QLabel {
            color: #fafafa;
        }
        QMessageBox QPushButton {
            background-color: #fafafa;
            color: #09090b;
            border-radius: 6px;
            padding: 6px 16px;
            font-weight: bold;
        }
    )";
    setStyleSheet(qss);
}

void KeySequenceWindow::closeEvent(QCloseEvent* event) {
    this->hide();
    event->ignore();
}