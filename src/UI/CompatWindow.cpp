#include "CompatWindow.h"
#include "ui_compatwindow.h"
#include <QPixmap>

CompatWindow::CompatWindow(std::shared_ptr<ISystemControl> systemControl, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::CompatWindow)
    , m_systemControl(std::move(systemControl)) {
    ui->setupUi(this);

    if (m_systemControl) {
        ui->sliderVolume->setValue(m_systemControl->getVolume());
        updateMediaInfo(m_systemControl->getMediaInfo());

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

void CompatWindow::onVolumeSliderChanged(int value) {
    if (m_systemControl) {
        m_systemControl->setVolume(value);
    }
}

void CompatWindow::onBtnPrevClicked() {
    if (m_systemControl) m_systemControl->runPlayerctl("previous");
}

void CompatWindow::onBtnPlayPauseClicked() {
    if (m_systemControl) m_systemControl->runPlayerctl("play-pause");
}

void CompatWindow::onBtnNextClicked() {
    if (m_systemControl) m_systemControl->runPlayerctl("next");
}

void CompatWindow::updateMediaInfo(const MediaInfo& info) {
    // Si no hay título, ocultar la ventana; de lo contrario, mostrarla.
    if (info.title.isEmpty()) {
        this->hide();
        return;
    }

    this->show();
    ui->labelTitle->setText(info.title);
    ui->labelArtist->setText(info.artist.isEmpty() ? "Desconocido" : info.artist);

    if (!info.artPath.isEmpty()) {
        ui->labelCover->setPixmap(QPixmap(info.artPath));
    }
    else {
        ui->labelCover->clear();
    }
}

void CompatWindow::updateVolume(int volume) {
    ui->sliderVolume->blockSignals(true);
    ui->sliderVolume->setValue(volume);
    ui->sliderVolume->blockSignals(false);
}