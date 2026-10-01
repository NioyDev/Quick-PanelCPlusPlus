#include "CompatWindow.h"
#include "ui_compatwindow.h"
#include <QPixmap>
#include <QGuiApplication>
#include <QScreen>

CompatWindow::CompatWindow(std::shared_ptr<ISystemControl> systemControl, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::CompatWindow)
    , m_systemControl(std::move(systemControl)) {
    ui->setupUi(this);

    // Ventana sin marco, siempre encima de todo y tratada como ventana emergente (Tool)
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);

    // Temporizador para ocultar automáticamente
    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &CompatWindow::hide);

    if (m_systemControl) {
        ui->sliderVolume->setValue(m_systemControl->getVolume());
        //updateMediaInfo(m_systemControl->getMediaInfo());
        connect(m_systemControl.get(), &ISystemControl::mediaInfoChanged,
            this, &CompatWindow::updateMediaInfo);
        connect(m_systemControl.get(), &ISystemControl::volumeChanged,
            this, &CompatWindow::updateVolume);
    }

    connect(ui->sliderVolume, &QSlider::valueChanged, this, &CompatWindow::onVolumeSliderChanged);
    connect(ui->btnPrev, &QPushButton::clicked, this, &CompatWindow::onBtnPrevClicked);
    connect(ui->btnPlayPause, &QPushButton::clicked, this, &CompatWindow::onBtnPlayPauseClicked);
    connect(ui->btnNext, &QPushButton::clicked, this, &CompatWindow::onBtnNextClicked);
}

CompatWindow::~CompatWindow() {
    delete ui;
}

void CompatWindow::positionTopLeft() {
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        // Ubicar en la esquina superior izquierda con un margen de 20px
        this->move(screenGeometry.x() + 20, screenGeometry.y() + 20);
    }
}

void CompatWindow::showTemporary(int durationMs) {
    positionTopLeft();
    this->show();
    m_hideTimer->start(durationMs);
}

void CompatWindow::closeEvent(QCloseEvent* event) {
    this->hide();
    m_hideTimer->stop();
    event->ignore();
}

void CompatWindow::enterEvent(QEnterEvent* event) {
    // Si el usuario pasa el mouse por encima, cancela el temporizador para que no se cierre mientras interactúa
    m_hideTimer->stop();
    QMainWindow::enterEvent(event);
}

void CompatWindow::leaveEvent(QEvent* event) {
    // Al quitar el mouse, espera 2 segundos y se oculta
    m_hideTimer->start(2000);
    QMainWindow::leaveEvent(event);
}

void CompatWindow::onVolumeSliderChanged(int value) {
    if (m_systemControl) {
        m_systemControl->setVolume(value);
    }
    m_hideTimer->start(3000);
}

void CompatWindow::onBtnPrevClicked() {
    if (m_systemControl) m_systemControl->runPlayerctl("previous");
    m_hideTimer->start(3000);
}

void CompatWindow::onBtnPlayPauseClicked() {
    if (m_systemControl) m_systemControl->runPlayerctl("play-pause");
    m_hideTimer->start(3000);
}

void CompatWindow::onBtnNextClicked() {
    if (m_systemControl) m_systemControl->runPlayerctl("next");
    m_hideTimer->start(3000);
}

void CompatWindow::updateMediaInfo(const MediaInfo& info) {
    if (info.title.isEmpty()) {
        this->hide();
        m_hideTimer->stop();
        return;
    }

    ui->labelTitle->setText(info.title);
    ui->labelArtist->setText(info.artist.isEmpty() ? "Desconocido" : info.artist);

    if (!info.artPath.isEmpty()) {
        ui->labelCover->setPixmap(QPixmap(info.artPath));
    }
    else {
        ui->labelCover->clear();
    }

    // Muestra la ventana en la esquina superior izquierda durante 3 segundos
    showTemporary(3000);
}

void CompatWindow::updateVolume(int volume) {
    ui->sliderVolume->blockSignals(true);
    ui->sliderVolume->setValue(volume);
    ui->sliderVolume->blockSignals(false);

    // Muestra la ventana al cambiar el volumen desde el sistema
    showTemporary(3000);
}