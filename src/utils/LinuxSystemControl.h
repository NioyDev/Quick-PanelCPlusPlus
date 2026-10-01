#pragma once

#include "Interfaz/ISystemControl.h"

class LinuxSystemControl : public ISystemControl {
    Q_OBJECT

public:
    explicit LinuxSystemControl(QObject* parent = nullptr);
    ~LinuxSystemControl() override = default;

    int getVolume() override;
    void setVolume(int volPct) override;
    MediaInfo getMediaInfo() override;
    void runPlayerctl(const QString& cmd) override;
};