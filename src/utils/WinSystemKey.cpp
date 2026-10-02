#include "WinSystemKey.h"
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QProcess>
#include <algorithm>

constexpr UINT VK_K = 0x4B;

WinSystemKey::WinSystemKey(QObject* parent) : ISystemKey(parent) {
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    m_storageFile = appData + "atajos_desc.json";

    setupGlobalHotkey();
    registerAllShortcuts();
}

WinSystemKey::~WinSystemKey() {
    unregisterAllShortcuts();
    removeGlobalHotkey();
}

void WinSystemKey::setupGlobalHotkey() {
    RegisterHotKey(NULL, HOTKEY_TOGGLE_ID, MOD_CONTROL | MOD_NOREPEAT, VK_K);
    if (auto app = QCoreApplication::instance()) {
        app->installNativeEventFilter(this);
    }
}

void WinSystemKey::removeGlobalHotkey() {
    if (auto app = QCoreApplication::instance()) {
        app->removeNativeEventFilter(this);
    }
    UnregisterHotKey(NULL, HOTKEY_TOGGLE_ID);
}

void WinSystemKey::registerAllShortcuts() {
    unregisterAllShortcuts();
    auto shortcuts = loadShortcuts();
    for (const auto& item : shortcuts) {
        UINT mods = 0, vk = 0;
        if (parseRawKey(item.rawKey, mods, vk)) {
            int id = m_nextId++;
            if (RegisterHotKey(NULL, id, mods, vk)) {
                m_rawKeyToId[item.rawKey] = id;
                m_idToCommand[id] = item.command;
            }
        }
    }
}

void WinSystemKey::unregisterAllShortcuts() {
    for (int id : m_rawKeyToId.values()) {
        UnregisterHotKey(NULL, id);
    }
    m_rawKeyToId.clear();
    m_idToCommand.clear();
}

bool WinSystemKey::parseRawKey(const QString& rawKey, UINT& outMods, UINT& outVk) {
    outMods = MOD_NOREPEAT;
    QString keyStr = rawKey;

    if (keyStr.contains("<Primary>")) { outMods |= MOD_CONTROL; keyStr.remove("<Primary>"); }
    if (keyStr.contains("<Alt>")) { outMods |= MOD_ALT;     keyStr.remove("<Alt>"); }
    if (keyStr.contains("<Shift>")) { outMods |= MOD_SHIFT;   keyStr.remove("<Shift>"); }
    if (keyStr.contains("<Super>")) { outMods |= MOD_WIN;     keyStr.remove("<Super>"); }

    keyStr = keyStr.trimmed().toUpper();
    if (keyStr.isEmpty()) return false;

    // Caracteres individuales (A-Z, 0-9)
    if (keyStr.length() == 1) {
        char ch = keyStr.at(0).toLatin1();
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            outVk = static_cast<UINT>(ch);
            return true;
        }
    }

    // Mapeo de teclas especiales comunes
    static const QMap<QString, UINT> specialKeys = {
        {"SPACE", VK_SPACE}, {"RETURN", VK_RETURN}, {"ENTER", VK_RETURN},
        {"TAB", VK_TAB}, {"ESC", VK_ESCAPE}, {"ESCAPE", VK_ESCAPE},
        {"BACKSPACE", VK_BACK}, {"DELETE", VK_DELETE}, {"INSERT", VK_INSERT},
        {"HOME", VK_HOME}, {"END", VK_END}, {"PAGEUP", VK_PRIOR}, {"PAGEDOWN", VK_NEXT},
        {"UP", VK_UP}, {"DOWN", VK_DOWN}, {"LEFT", VK_LEFT}, {"RIGHT", VK_RIGHT},
        {"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3}, {"F4", VK_F4},
        {"F5", VK_F5}, {"F6", VK_F6}, {"F7", VK_F7}, {"F8", VK_F8},
        {"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12}
    };

    if (specialKeys.contains(keyStr)) {
        outVk = specialKeys[keyStr];
        return true;
    }

    // Fallback usando VkKeyScanW para símbolos de puntuación
    SHORT vkScan = VkKeyScanW(keyStr.at(0).unicode());
    if (vkScan != -1) {
        outVk = LOBYTE(vkScan);
        return true;
    }

    return false;
}

void WinSystemKey::executeCommand(const QString& command) {
    if (command.isEmpty()) return;

    // Ejecución silenciosa y no bloqueante mediante PowerShell
    QProcess::startDetached("powershell.exe", QStringList()
        << "-NoProfile"
        << "-ExecutionPolicy" << "Bypass"
        << "-WindowStyle" << "Hidden"
        << "-Command" << command);
}

bool WinSystemKey::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    Q_UNUSED(result);
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            int hotkeyId = static_cast<int>(msg->wParam);

            // 1. Interruptor global de la interfaz
            if (hotkeyId == HOTKEY_TOGGLE_ID) {
                emit toggleRequested();
                return true;
            }

            // 2. Atajos de comandos definidos por el usuario
            if (m_idToCommand.contains(hotkeyId)) {
                executeCommand(m_idToCommand[hotkeyId]);
                return true;
            }
        }
    }
    return false;
}

bool WinSystemKey::getKeyRepeatEnabled() {
    return true;
}

void WinSystemKey::setKeyRepeatEnabled(bool) {}

int WinSystemKey::getKeyRepeatDelay() {
    DWORD delay = 0;
    if (SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &delay, 0)) {
        return static_cast<int>((delay + 1) * 250);
    }
    return 500;
}

void WinSystemKey::setKeyRepeatDelay(int delayMs) {
    DWORD val = static_cast<DWORD>((delayMs / 250) - 1);
    SystemParametersInfo(SPI_SETKEYBOARDDELAY, std::clamp(val, 0UL, 3UL), nullptr, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}

int WinSystemKey::getKeyRepeatRate() {
    DWORD speed = 0;
    if (SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &speed, 0)) {
        return static_cast<int>(speed);
    }
    return 20;
}

void WinSystemKey::setKeyRepeatRate(int rate) {
    DWORD val = static_cast<DWORD>(rate);
    SystemParametersInfo(SPI_SETKEYBOARDSPEED, std::clamp(val, 0UL, 31UL), nullptr, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}

QString WinSystemKey::getCurrentLayout() {
    return "es";
}

void WinSystemKey::setLayout(const QString&) {}

QJsonObject WinSystemKey::loadStore() {
    QFile f(m_storageFile);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

void WinSystemKey::saveStore(const QJsonObject& json) {
    QFile f(m_storageFile);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(json).toJson());
    }
}

QList<ShortcutItem> WinSystemKey::loadShortcuts() {
    QList<ShortcutItem> list;
    QJsonObject store = loadStore();
    for (auto it = store.begin(); it != store.end(); ++it) {
        QJsonObject obj = it.value().toObject();
        list.append({
            obj["displayKey"].toString(),
            it.key(),
            obj["command"].toString(),
            obj["description"].toString()
            });
    }
    return list;
}

bool WinSystemKey::addShortcut(const ShortcutItem& item) {
    // 1. Guardar o actualizar en JSON
    QJsonObject store = loadStore();
    QJsonObject obj;
    obj["displayKey"] = item.displayKey;
    obj["command"] = item.command;
    obj["description"] = item.description;
    store[item.rawKey] = obj;
    saveStore(store);

    // 2. Si ya existía un hotkey con esta tecla, eliminar el registro previo
    if (m_rawKeyToId.contains(item.rawKey)) {
        int oldId = m_rawKeyToId.take(item.rawKey);
        UnregisterHotKey(NULL, oldId);
        m_idToCommand.remove(oldId);
    }

    // 3. Registrar dinámicamente en Windows OS
    UINT mods = 0, vk = 0;
    if (parseRawKey(item.rawKey, mods, vk)) {
        int newId = m_nextId++;
        if (RegisterHotKey(NULL, newId, mods, vk)) {
            m_rawKeyToId[item.rawKey] = newId;
            m_idToCommand[newId] = item.command;
        }
    }
    return true;
}

bool WinSystemKey::removeShortcut(const QString& rawKey) {
    // 1. Remover del JSON
    QJsonObject store = loadStore();
    store.remove(rawKey);
    saveStore(store);

    // 2. Desregistrar de Windows OS y limpiar mapas
    if (m_rawKeyToId.contains(rawKey)) {
        int id = m_rawKeyToId.take(rawKey);
        UnregisterHotKey(NULL, id);
        m_idToCommand.remove(id);
    }
    return true;
}