#pragma once

#include <QObject>
#include <QString>
#include <QList>

struct ShortcutItem {
    QString displayKey; // Ej: "Ctrl + D"
    QString rawKey;     // Ej: "<Primary>d"
    QString command;    // Ej: "alacritty"
    QString description;// Ej: "Abrir Terminal"
};

class ISystemKey : public QObject {
    Q_OBJECT

public:
    explicit ISystemKey(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ISystemKey() = default;

    // Repetición de teclado
    virtual bool getKeyRepeatEnabled() = 0;
    virtual void setKeyRepeatEnabled(bool enabled) = 0;
    virtual int getKeyRepeatDelay() = 0;
    virtual void setKeyRepeatDelay(int delayMs) = 0;
    virtual int getKeyRepeatRate() = 0;
    virtual void setKeyRepeatRate(int rate) = 0;

    // Distribución
    virtual QString getCurrentLayout() = 0;
    virtual void setLayout(const QString& layoutId) = 0;

    // Atajos de teclado
    virtual QList<ShortcutItem> loadShortcuts() = 0;
    virtual bool addShortcut(const ShortcutItem& item) = 0;
    virtual bool removeShortcut(const QString& rawKey) = 0;

signals:
    void toggleRequested();
};