#pragma once

#include "Interfaz/ISystemControl.h"
#include <QProcess>

class LinuxSystemControl : public ISystemControl {
    Q_OBJECT

public:
    explicit LinuxSystemControl(QObject* parent = nullptr);
    ~LinuxSystemControl() override;

    int getVolume() override;
    void setVolume(int volPct) override;
    MediaInfo getMediaInfo() override;
    void runPlayerctl(const QString& cmd) override;

private:
    void setupVolumeEvents();

    QProcess* m_pactlProcess{ nullptr };
};