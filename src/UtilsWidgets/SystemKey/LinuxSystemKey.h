#pragma once

#include "Interfaz/ISystemKey.h"
#include <QAbstractNativeEventFilter>
#include <QJsonObject>
#include <cstdint>

class LinuxSystemKey : public ISystemKey, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit LinuxSystemKey(QObject* parent = nullptr);
    ~LinuxSystemKey() override;

    // Repetición de teclado
    bool getKeyRepeatEnabled() override;
    void setKeyRepeatEnabled(bool enabled) override;
    int getKeyRepeatDelay() override;
    void setKeyRepeatDelay(int delayMs) override;
    int getKeyRepeatRate() override;
    void setKeyRepeatRate(int rate) override;

    // Distribución
    QString getCurrentLayout() override;
    void setLayout(const QString& layoutId) override;

    // Atajos de teclado
    QList<ShortcutItem> loadShortcuts() override;
    bool addShortcut(const ShortcutItem& item) override;
    bool removeShortcut(const QString& rawKey) override;

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void setupGlobalHotkey();
    void removeGlobalHotkey();

    QString m_descFilePath;
    uint8_t m_keyCodeK{ 0 };

    QJsonObject loadDescriptions();
    void saveDescriptions(const QJsonObject& json);
    void applyXsetRate(int delay, int rate);
    QString humanizeKey(const QString& rawKey);
};