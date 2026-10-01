#pragma once

#include "Interfaz/ISystemControl.h"
#include <winrt/Windows.Media.Control.h>

class WinSystemControl : public ISystemControl {
    Q_OBJECT

public:
    explicit WinSystemControl(QObject* parent = nullptr);
    ~WinSystemControl() override = default;

    int getVolume() override;
    void setVolume(int volPct) override;
    MediaInfo getMediaInfo() override;
    void runPlayerctl(const QString& cmd) override;

private:
    void setupMediaEvents();
    MediaInfo extractMediaProperties(winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionMediaProperties const& props);

    winrt::event_token m_mediaToken;
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession m_currentSession{ nullptr };
};