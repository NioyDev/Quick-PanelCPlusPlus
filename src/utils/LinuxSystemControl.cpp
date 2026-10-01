#include "LinuxSystemControl.h"
#include <QProcess>

LinuxSystemControl::LinuxSystemControl(QObject* parent) : ISystemControl(parent) {}

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