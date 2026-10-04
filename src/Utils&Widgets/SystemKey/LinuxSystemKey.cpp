#include "LinuxSystemKey.h"
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QCoreApplication>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <xcb/xcb.h>

LinuxSystemKey::LinuxSystemKey(QObject* parent) : ISystemKey(parent) {
    m_descFilePath = QDir::homePath() + "atajos_desc.json";
    setupGlobalHotkey();
}

LinuxSystemKey::~LinuxSystemKey() {
    removeGlobalHotkey();
}

void LinuxSystemKey::setupGlobalHotkey() {
    Display* dpy = XOpenDisplay(NULL);
    if (dpy) {
        Window root = DefaultRootWindow(dpy);
        KeyCode keycode = XKeysymToKeycode(dpy, XK_k);
        m_keyCodeK = keycode;

        // Capturar Super + K con combinaciones de modBloq/NumLock
        XGrabKey(dpy, keycode, Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, keycode, Mod4Mask | LockMask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, keycode, Mod4Mask | Mod2Mask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, keycode, Mod4Mask | LockMask | Mod2Mask, root, True, GrabModeAsync, GrabModeAsync);
        XFlush(dpy);
        XCloseDisplay(dpy);
    }

    if (auto app = QCoreApplication::instance()) {
        app->installNativeEventFilter(this);
    }
}

void LinuxSystemKey::removeGlobalHotkey() {
    if (auto app = QCoreApplication::instance()) {
        app->removeNativeEventFilter(this);
    }

    Display* dpy = XOpenDisplay(NULL);
    if (dpy) {
        Window root = DefaultRootWindow(dpy);
        KeyCode keycode = XKeysymToKeycode(dpy, XK_k);
        XUngrabKey(dpy, keycode, Mod4Mask, root);
        XUngrabKey(dpy, keycode, Mod4Mask | LockMask, root);
        XUngrabKey(dpy, keycode, Mod4Mask | Mod2Mask, root);
        XUngrabKey(dpy, keycode, Mod4Mask | LockMask | Mod2Mask, root);
        XFlush(dpy);
        XCloseDisplay(dpy);
    }
}

bool LinuxSystemKey::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    Q_UNUSED(result);
    if (eventType == "xcb_generic_event_t") {
        auto* event = static_cast<xcb_generic_event_t*>(message);
        uint8_t responseType = event->response_type & ~0x80;
        if (responseType == XCB_KEY_PRESS) {
            auto* keyEvent = reinterpret_cast<xcb_key_press_event_t*>(event);
            if (keyEvent->detail == m_keyCodeK && (keyEvent->state & XCB_MOD_MASK_4)) {
                emit toggleRequested();
                return true;
            }
        }
    }
    return false;
}

bool LinuxSystemKey::getKeyRepeatEnabled() {
    QProcess proc;
    proc.start("xfconf-query", { "-c", "keyboards", "-p", "/Default/KeyRepeat" });
    proc.waitForFinished();
    return proc.readAllStandardOutput().trimmed() == "true";
}

void LinuxSystemKey::setKeyRepeatEnabled(bool enabled) {
    QString val = enabled ? "true" : "false";
    QProcess::execute("xfconf-query", { "-c", "keyboards", "-p", "/Default/KeyRepeat", "-n", "-t", "bool", "-s", val });
    QProcess::execute("xset", { "r", enabled ? "on" : "off" });
}

int LinuxSystemKey::getKeyRepeatDelay() {
    QProcess proc;
    proc.start("xfconf-query", { "-c", "keyboards", "-p", "/Default/KeyRepeatDelay" });
    proc.waitForFinished();
    bool ok;
    int val = proc.readAllStandardOutput().trimmed().toInt(&ok);
    return ok ? val : 500;
}

void LinuxSystemKey::setKeyRepeatDelay(int delayMs) {
    QProcess::execute("xfconf-query", { "-c", "keyboards", "-p", "/Default/KeyRepeatDelay", "-n", "-t", "int", "-s", QString::number(delayMs) });
    applyXsetRate(delayMs, getKeyRepeatRate());
}

int LinuxSystemKey::getKeyRepeatRate() {
    QProcess proc;
    proc.start("xfconf-query", { "-c", "keyboards", "-p", "/Default/KeyRepeatRate" });
    proc.waitForFinished();
    bool ok;
    int val = proc.readAllStandardOutput().trimmed().toInt(&ok);
    return ok ? val : 20;
}

void LinuxSystemKey::setKeyRepeatRate(int rate) {
    QProcess::execute("xfconf-query", { "-c", "keyboards", "-p", "/Default/KeyRepeatRate", "-n", "-t", "int", "-s", QString::number(rate) });
    applyXsetRate(getKeyRepeatDelay(), rate);
}

void LinuxSystemKey::applyXsetRate(int delay, int rate) {
    QProcess::execute("xset", { "r", "rate", QString::number(delay), QString::number(rate) });
}

QString LinuxSystemKey::getCurrentLayout() {
    QProcess proc;
    proc.start("setxkbmap", { "-query" });
    proc.waitForFinished();
    QString out = proc.readAllStandardOutput();
    for (const QString& line : out.split('\n')) {
        if (line.startsWith("layout:")) {
            return line.section(':', 1).trimmed();
        }
    }
    return "es";
}

void LinuxSystemKey::setLayout(const QString& layoutId) {
    QProcess::execute("setxkbmap", { layoutId });
    QProcess::execute("xfconf-query", { "-c", "keyboard-layout", "-p", "/Default/XkbDisable", "-n", "-t", "bool", "-s", "false" });
    QProcess::execute("xfconf-query", { "-c", "keyboard-layout", "-p", "/Default/XkbLayout", "-n", "-t", "string", "-s", layoutId });
}

QJsonObject LinuxSystemKey::loadDescriptions() {
    QFile file(m_descFilePath);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

void LinuxSystemKey::saveDescriptions(const QJsonObject& json) {
    QFile file(m_descFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(json).toJson());
    }
}

QString LinuxSystemKey::humanizeKey(const QString& rawKey) {
    QString res = rawKey;
    static const QMap<QString, QString> map = {
        {"<Primary>", "Ctrl + "},
        {"<Control>", "Ctrl + "},
        {"<Alt>", "Alt + "},
        {"<Shift>", "Shift + "},
        {"<Super>", "Super + "}
    };
    for (auto it = map.begin(); it != map.end(); ++it) {
        res.replace(it.key(), it.value());
    }
    return res;
}

QList<ShortcutItem> LinuxSystemKey::loadShortcuts() {
    QList<ShortcutItem> items;
    QJsonObject descs = loadDescriptions();

    QProcess proc;
    proc.start("xfconf-query", { "-c", "xfce4-keyboard-shortcuts", "-l", "-v" });
    proc.waitForFinished();
    QString out = proc.readAllStandardOutput();

    for (const QString& line : out.split('\n')) {
        if (line.contains("/commands/custom/")) {
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                QString rawKey = parts[0].section("/commands/custom/", 1);
                QString cmd = parts.mid(1).join(" ");
                if (cmd.trimmed() == "startup-notify") continue;

                ShortcutItem item;
                item.rawKey = rawKey;
                item.command = cmd;
                item.description = descs.value(rawKey).toString();
                item.displayKey = humanizeKey(rawKey);
                items.append(item);
            }
        }
    }
    return items;
}

bool LinuxSystemKey::addShortcut(const ShortcutItem& item) {
    QProcess::execute("xfconf-query", {
        "-c", "xfce4-keyboard-shortcuts",
        "-p", "/commands/custom/" + item.rawKey,
        "-n", "-t", "string", "-s", item.command
        });

    QJsonObject descs = loadDescriptions();
    descs[item.rawKey] = item.description;
    saveDescriptions(descs);
    return true;
}

bool LinuxSystemKey::removeShortcut(const QString& rawKey) {
    QProcess::execute("xfconf-query", {
        "-c", "xfce4-keyboard-shortcuts",
        "-p", "/commands/custom/" + rawKey,
        "-r", "-R"
        });

    QJsonObject descs = loadDescriptions();
    descs.remove(rawKey);
    saveDescriptions(descs);
    return true;
}