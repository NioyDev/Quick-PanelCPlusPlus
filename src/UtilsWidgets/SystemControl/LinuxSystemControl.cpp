#include "LinuxSystemControl.h"
#include <QProcess>

LinuxSystemControl::LinuxSystemControl(QObject* parent) : ISystemControl(parent) {
    setupVolumeEvents();
}

LinuxSystemControl::~LinuxSystemControl() {
    if (m_pactlProcess) {
        m_pactlProcess->kill();
        m_pactlProcess->deleteLater();
    }
}

void LinuxSystemControl::setupVolumeEvents() {
    m_pactlProcess = new QProcess(this);
    // Escucha eventos de PulseAudio / PipeWire en tiempo real
    m_pactlProcess->start("pactl", QStringList() << "subscribe");

    connect(m_pactlProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        QString output = m_pactlProcess->readAllStandardOutput();
        // Si el evento reportado corresponde al cambio en un dispositivo de salida ('sink')
        if (output.contains("Event 'change' on sink")) {
            int currentVol = getVolume();
            emit volumeChanged(currentVol);
        }
        });
}

int LinuxSystemControl::getVolume() {
    QProcess process;
    process.start("pamixer", QStringList() << "--get-volume");
    process.waitForFinished();
    QString output = process.readAllStandardOutput().trimmed();
    bool ok;
    int vol = output.toInt(&ok);
    return ok ? vol : 50;
}

void LinuxSystemControl::setVolume(int volPct) {
    QProcess::startDetached("pamixer", QStringList() << "--set-volume" << QString::number(volPct));
}

MediaInfo LinuxSystemControl::getMediaInfo() {
    MediaInfo info{ "", "", "" };

    QProcess procTitle, procArtist, procArt;
    procTitle.start("playerctl", QStringList() << "metadata" << "title");
    procTitle.waitForFinished();
    info.title = procTitle.readAllStandardOutput().trimmed();

    procArtist.start("playerctl", QStringList() << "metadata" << "artist");
    procArtist.waitForFinished();
    info.artist = procArtist.readAllStandardOutput().trimmed();

    procArt.start("playerctl", QStringList() << "metadata" << "mpris:artUrl");
    procArt.waitForFinished();
    info.artPath = procArt.readAllStandardOutput().trimmed().replace("file://", "");

    return info;
}

void LinuxSystemControl::runPlayerctl(const QString& cmd) {
    QProcess::startDetached("playerctl", QStringList() << cmd);
}