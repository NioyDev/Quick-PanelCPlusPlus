#pragma once

#include <QObject>
#include <QString>

struct MediaInfo {
    QString title;
    QString artist;
    QString artPath;
};

class ISystemControl : public QObject {
    Q_OBJECT

public:
    explicit ISystemControl(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ISystemControl() = default;

    // Métodos virtuales puros (Acciones)
    virtual int getVolume() = 0;
    virtual void setVolume(int volPct) = 0;
    virtual void runPlayerctl(const QString& cmd) = 0;
    virtual MediaInfo getMediaInfo() = 0;

signals:
    // Señales enviadas a la UI cuando ocurre un cambio en el SO
    void mediaInfoChanged(const MediaInfo& info);
    void volumeChanged(int newVolume);
};