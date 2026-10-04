#pragma once

#include "Interfaz/ISystemKey.h"
#include <QAbstractNativeEventFilter>
#include <QJsonObject>
#include <QMap>
#include <windows.h>

class WinSystemKey : public ISystemKey, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit WinSystemKey(QObject* parent = nullptr);
    ~WinSystemKey() override;

    bool getKeyRepeatEnabled() override;
    void setKeyRepeatEnabled(bool enabled) override;
    int getKeyRepeatDelay() override;
    void setKeyRepeatDelay(int delayMs) override;
    int getKeyRepeatRate() override;
    void setKeyRepeatRate(int rate) override;

    QString getCurrentLayout() override;
    void setLayout(const QString& layoutId) override;

    QList<ShortcutItem> loadShortcuts() override;
    bool addShortcut(const ShortcutItem& item) override;
    bool removeShortcut(const QString& rawKey) override;

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void setupGlobalHotkey();
    void removeGlobalHotkey();

    void registerAllShortcuts();
    void unregisterAllShortcuts();
    bool parseRawKey(const QString& rawKey, UINT& outMods, UINT& outVk);
    void executeCommand(const QString& command);

    QString m_storageFile;
    QJsonObject loadStore();
    void saveStore(const QJsonObject& json);

    QMap<int, QString> m_idToCommand;
    QMap<QString, int> m_rawKeyToId;
    int m_nextId{ 0x6000 };

    static constexpr int HOTKEY_TOGGLE_ID = 0x574B;
};