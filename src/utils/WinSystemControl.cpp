#include "WinSystemControl.h"

#include <unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iostream>
#include <future>
#include <QMetaObject>

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Storage::Streams;

WinSystemControl::WinSystemControl(QObject* parent) : ISystemControl(parent) {
    setupMediaEvents();
}

int WinSystemControl::getVolume() {
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr) || !enumerator) return 50;

    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
    enumerator->Release();
    if (FAILED(hr) || !device) return 50;

    IAudioEndpointVolume* volume = nullptr;
    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr, (void**)&volume);
    device->Release();
    if (FAILED(hr) || !volume) return 50;

    float currentVal = 0.0f;
    volume->GetMasterVolumeLevelScalar(&currentVal);
    volume->Release();

    return static_cast<int>(currentVal * 100.0f);
}

void WinSystemControl::setVolume(int volPct) {
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr) || !enumerator) return;

    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
    enumerator->Release();
    if (FAILED(hr) || !device) return;

    IAudioEndpointVolume* volume = nullptr;
    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr, (void**)&volume);
    device->Release();
    if (FAILED(hr) || !volume) return;

    float targetVal = static_cast<float>(volPct) / 100.0f;
    targetVal = std::clamp(targetVal, 0.0f, 1.0f);

    volume->SetMasterVolumeLevelScalar(targetVal, nullptr);
    volume->Release();
}

MediaInfo WinSystemControl::getMediaInfo() {
    return std::async(std::launch::async, [this]() -> MediaInfo {
        MediaInfo info{ "", "", "" };
        try {
            auto manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
            if (!manager) return info;

            auto session = manager.GetCurrentSession();
            if (session) {
                auto props = session.TryGetMediaPropertiesAsync().get();
                if (props) {
                    info = extractMediaProperties(props);
                }
            }
        }
        catch (...) {}
        return info;
        }).get();
}

MediaInfo WinSystemControl::extractMediaProperties(GlobalSystemMediaTransportControlsSessionMediaProperties const& props) {
    MediaInfo info{ "", "", "" };
    info.title = QString::fromStdWString(props.Title().c_str());
    info.artist = QString::fromStdWString(props.Artist().c_str());

    auto thumbnail = props.Thumbnail();
    if (thumbnail) {
        try {
            auto stream = thumbnail.OpenReadAsync().get();
            if (stream && stream.Size() > 0) {
                uint64_t size = stream.Size();
                Buffer buffer(static_cast<uint32_t>(size));
                stream.ReadAsync(buffer, static_cast<uint32_t>(size), InputStreamOptions::None).get();

                wchar_t tempDir[MAX_PATH];
                GetTempPathW(MAX_PATH, tempDir);
                std::wstring tempArtPath = std::wstring(tempDir) + L"win_osd_art.jpg";

                DataReader reader = DataReader::FromBuffer(buffer);
                std::vector<uint8_t> bytes(size);
                reader.ReadBytes(bytes);

                std::ofstream file(tempArtPath, std::ios::binary);
                if (file.is_open()) {
                    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                    file.close();
                    info.artPath = QString::fromStdWString(tempArtPath);
                }
            }
        }
        catch (...) {}
    }
    return info;
}

void WinSystemControl::setupMediaEvents() {
    std::async(std::launch::async, [this]() {
        try {
            auto manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
            if (!manager) return;

            auto updateSession = [this](GlobalSystemMediaTransportControlsSession const& session) {
                if (!session) return;

                m_mediaToken = session.MediaPropertiesChanged([this](GlobalSystemMediaTransportControlsSession const& sender, auto const&) {
                    try {
                        auto props = sender.TryGetMediaPropertiesAsync().get();
                        if (props) {
                            MediaInfo info = this->extractMediaProperties(props);

                            QMetaObject::invokeMethod(this, [this, info]() {
                                emit mediaInfoChanged(info);
                                });
                        }
                    }
                    catch (...) {}
                    });
                };

            m_currentSession = manager.GetCurrentSession();
            updateSession(m_currentSession);

            manager.CurrentSessionChanged([this, manager, updateSession](auto const&, auto const&) {
                m_currentSession = manager.GetCurrentSession();
                updateSession(m_currentSession);
                });
        }
        catch (...) {}
        });
}

void WinSystemControl::runPlayerctl(const QString& cmd) {
    std::async(std::launch::async, [cmd]() {
        try {
            auto manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
            if (!manager) return;

            auto session = manager.GetCurrentSession();
            if (session) {
                if (cmd == "previous") session.TrySkipPreviousAsync().get();
                else if (cmd == "next") session.TrySkipNextAsync().get();
                else if (cmd == "play-pause") session.TryTogglePlayPauseAsync().get();
            }
        }
        catch (...) {}
        });
}