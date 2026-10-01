#pragma once

#include "Interfaz/ISystemControl.h"
#include <winrt/Windows.Media.Control.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

// Callback nativo de Win32 para cambios de volumen en el sistema
class VolumeCallback : public IAudioEndpointVolumeCallback {
public:
    using CallbackFunc = std::function<void(int)>;

    explicit VolumeCallback(CallbackFunc callback) : m_refCount(1), m_callback(std::move(callback)) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IAudioEndpointVolumeCallback)) {
            *ppv = static_cast<IAudioEndpointVolumeCallback*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }

    STDMETHODIMP_(ULONG) Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }

    STDMETHODIMP OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) override {
        if (pNotify && m_callback) {
            int vol = static_cast<int>(pNotify->fMasterVolume * 100.0f + 0.5f);
            m_callback(vol);
        }
        return S_OK;
    }

private:
    LONG m_refCount;
    CallbackFunc m_callback;
};

class WinSystemControl : public ISystemControl {
    Q_OBJECT

public:
    explicit WinSystemControl(QObject* parent = nullptr);
    ~WinSystemControl() override;

    int getVolume() override;
    void setVolume(int volPct) override;
    MediaInfo getMediaInfo() override;
    void runPlayerctl(const QString& cmd) override;

private:
    // Estado compartido con los callbacks: sobrevive aunque 'this' se destruya
    struct Shared {
        std::mutex mutex;
        std::atomic<bool> alive{ true };
    };

    void setupMediaEvents();
    void setupVolumeEvents();
    // Requiere tener tomado m_shared->mutex
    void attachSession(winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession const& session);
    MediaInfo extractMediaProperties(winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionMediaProperties const& props);

    std::shared_ptr<Shared> m_shared{ std::make_shared<Shared>() };

    // Tokens y referencias de WinRT para cancelación de suscripciones
    winrt::event_token m_mediaToken{};
    winrt::event_token m_sessionToken{};
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager m_manager{ nullptr };
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession m_currentSession{ nullptr };

    // Carátula temporal
    std::mutex m_artMutex;
    std::atomic<unsigned> m_artCounter{ 0 };
    std::wstring m_lastArtPath;

    // COM WASAPI
    IAudioEndpointVolume* m_endpointVolume{ nullptr };
    VolumeCallback* m_volumeCallback{ nullptr };
};