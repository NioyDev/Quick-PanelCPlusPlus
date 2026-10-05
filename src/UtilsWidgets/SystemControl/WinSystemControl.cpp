#include "WinSystemControl.h"

#include <unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

#include <windows.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iostream>
#include <future>
#include <QMetaObject>

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Storage::Streams;

namespace {

    // Los hilos de std::async no inicializan WinRT. Si ya estaba inicializado
    // (o con otro modelo) init_apartment lanza; lo ignoramos.
    void ensureMta() {
        try { winrt::init_apartment(winrt::apartment_type::multi_threaded); }
        catch (...) {}
    }

    // Corrutina fire-and-forget: no bloquea el hilo de UI ni usa .get()
    winrt::fire_and_forget runCommandAsync(QString cmd) {
        co_await winrt::resume_background();
        try {
            auto manager = co_await GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
            if (!manager) co_return;

            auto session = manager.GetCurrentSession();
            if (!session) co_return;

            if (cmd == "previous")        co_await session.TrySkipPreviousAsync();
            else if (cmd == "next")       co_await session.TrySkipNextAsync();
            else if (cmd == "play-pause") co_await session.TryTogglePlayPauseAsync();
        }
        catch (...) {}
    }

} // namespace

WinSystemControl::WinSystemControl(QObject* parent) : ISystemControl(parent) {
    // Nota: el hilo principal de Qt ya tiene COM inicializado (STA).
    // No llamar init_apartment aquí.
    setupVolumeEvents();
    setupMediaEvents();
}

WinSystemControl::~WinSystemControl() {
    // 1. Marcar como muerto para que los callbacks pendientes salgan sin tocar 'this'
    {
        std::lock_guard<std::mutex> lk(m_artMutex);
        if (!m_lastArtPath.empty()) { DeleteFileW(m_lastArtPath.c_str()); m_lastArtPath.clear(); }
    }

    // 2. Desregistrar el callback de volumen (espera a que terminen los callbacks en curso)
    if (m_endpointVolume) {
        if (m_volumeCallback) {
            m_endpointVolume->UnregisterControlChangeNotify(m_volumeCallback);
            m_volumeCallback->Release();
            m_volumeCallback = nullptr;
        }
        m_endpointVolume->Release();
        m_endpointVolume = nullptr;
    }

    // 3. Cancelar eventos WinRT y liberar objetos mientras COM sigue vivo
    std::lock_guard<std::mutex> lk(m_shared->mutex);
    try {
        if (m_currentSession && m_mediaToken) m_currentSession.MediaPropertiesChanged(m_mediaToken);
    }
    catch (...) {}
    try {
        if (m_manager && m_sessionToken) m_manager.CurrentSessionChanged(m_sessionToken);
    }
    catch (...) {}
    m_mediaToken = {};
    m_sessionToken = {};
    m_currentSession = nullptr;
    m_manager = nullptr;
}

void WinSystemControl::setupVolumeEvents() {
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr) || !enumerator) return;

    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
    enumerator->Release();
    if (FAILED(hr) || !device) return;

    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr, (void**)&m_endpointVolume);
    device->Release();
    if (FAILED(hr) || !m_endpointVolume) return;

    m_volumeCallback = new VolumeCallback([this](int newVolume) {
        QMetaObject::invokeMethod(this, [this, newVolume]() {
            emit volumeChanged(newVolume);
            });
        });

    hr = m_endpointVolume->RegisterControlChangeNotify(m_volumeCallback);
    if (FAILED(hr)) {
        m_volumeCallback->Release();
        m_volumeCallback = nullptr;
    }
}

int WinSystemControl::getVolume() {
    if (m_endpointVolume) {
        float currentVal = 0.0f;
        if (SUCCEEDED(m_endpointVolume->GetMasterVolumeLevelScalar(&currentVal))) {
            return static_cast<int>(currentVal * 100.0f + 0.5f);
        }
    }
    return 50;
}

void WinSystemControl::setVolume(int volPct) {
    if (m_endpointVolume) {
        float targetVal = static_cast<float>(volPct) / 100.0f;
        targetVal = std::clamp(targetVal, 0.0f, 1.0f);
        m_endpointVolume->SetMasterVolumeLevelScalar(targetVal, nullptr);
    }
}

MediaInfo WinSystemControl::getMediaInfo() {
    // Se ejecuta en otro hilo porque .get() no debe llamarse en el hilo STA de UI.
    // (La UI sigue esperando el resultado; si molesta, usar mediaInfoChanged.)
    return std::async(std::launch::async, [this]() -> MediaInfo {
        ensureMta();
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
    info.title = QString::fromStdWString(std::wstring(props.Title()));
    info.artist = QString::fromStdWString(std::wstring(props.Artist()));

    auto thumbnail = props.Thumbnail();
    if (thumbnail) {
        try {
            auto stream = thumbnail.OpenReadAsync().get();
            if (stream && stream.Size() > 0) {
                uint32_t size = static_cast<uint32_t>(stream.Size());
                Buffer buffer(size);
                stream.ReadAsync(buffer, size, InputStreamOptions::None).get();

                DataReader reader = DataReader::FromBuffer(buffer);
                std::vector<uint8_t> bytes(buffer.Length());
                reader.ReadBytes(bytes);

                wchar_t tempDir[MAX_PATH];
                GetTempPathW(MAX_PATH, tempDir);
                // Nombre único por carátula: evita carreras si llegan dos eventos seguidos
                std::wstring tempArtPath = std::wstring(tempDir) + L"win_osd_art_" +
                    std::to_wstring(m_artCounter.fetch_add(1)) + L".jpg";

                std::ofstream file(tempArtPath, std::ios::binary);
                if (file.is_open()) {
                    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                    file.close();
                    info.artPath = QString::fromStdWString(tempArtPath);

                    // Borrar la carátula anterior
                    std::lock_guard<std::mutex> lk(m_artMutex);
                    if (!m_lastArtPath.empty()) DeleteFileW(m_lastArtPath.c_str());
                    m_lastArtPath = tempArtPath;
                }
            }
        }
        catch (...) {}
    }
    return info;
}

// Requiere m_shared->mutex tomado por el llamador
void WinSystemControl::attachSession(GlobalSystemMediaTransportControlsSession const& session) {
    // Soltar la suscripción de la sesión anterior
    if (m_currentSession && m_mediaToken) {
        try { m_currentSession.MediaPropertiesChanged(m_mediaToken); }
        catch (...) {}
        m_mediaToken = {};
    }

    m_currentSession = session;
    if (!m_currentSession) return;

    auto shared = m_shared;
    m_mediaToken = m_currentSession.MediaPropertiesChanged(
        [this, shared](GlobalSystemMediaTransportControlsSession const& sender, auto const&) {
            // El lock evita que el destructor libere 'this' mientras trabajamos
            std::lock_guard<std::mutex> lk(shared->mutex);
            if (!shared->alive) return;
            try {
                auto props = sender.TryGetMediaPropertiesAsync().get();
                if (props) {
                    MediaInfo info = extractMediaProperties(props);
                    QMetaObject::invokeMethod(this, [this, info]() {
                        emit mediaInfoChanged(info);
                        });
                }
            }
            catch (...) {}
        });
}

void WinSystemControl::setupMediaEvents() {
    [](WinSystemControl* self, std::shared_ptr<Shared> shared) -> winrt::fire_and_forget {
        co_await winrt::resume_background();

        // Al arrancar con Windows, RequestAsync puede fallar los primeros segundos
        GlobalSystemMediaTransportControlsSessionManager manager{ nullptr };
        for (int attempt = 0; attempt < 5 && !manager; ++attempt) {
            try { manager = co_await GlobalSystemMediaTransportControlsSessionManager::RequestAsync(); }
            catch (...) {}
            if (!manager) co_await winrt::resume_after(std::chrono::seconds(3));
            if (!shared->alive) co_return;
        }
        if (!manager) co_return;

        GlobalSystemMediaTransportControlsSession session{ nullptr };
        {   // NO mantener el lock a través de un co_await (puede reanudar en otro hilo)
            std::lock_guard<std::mutex> lk(shared->mutex);
            if (!shared->alive) co_return;
            self->m_manager = manager;
            self->attachSession(manager.GetCurrentSession());
            self->m_sessionToken = manager.CurrentSessionChanged(
                [self, shared](auto const&, auto const&) {
                    std::lock_guard<std::mutex> lk2(shared->mutex);
                    if (!shared->alive || !self->m_manager) return;
                    self->attachSession(self->m_manager.GetCurrentSession());
                });
            session = self->m_currentSession;
        }

        // Estado inicial (los eventos solo avisan de cambios posteriores)
        try {
            if (session) {
                auto props = co_await session.TryGetMediaPropertiesAsync();
                std::lock_guard<std::mutex> lk(shared->mutex);
                if (!shared->alive || !props) co_return;
                MediaInfo info = self->extractMediaProperties(props);
                QMetaObject::invokeMethod(self, [self, info]() { emit self->mediaInfoChanged(info); });
            }
        }
        catch (...) {}
        }(this, m_shared);
}

void WinSystemControl::runPlayerctl(const QString& cmd) {
    runCommandAsync(cmd);
}