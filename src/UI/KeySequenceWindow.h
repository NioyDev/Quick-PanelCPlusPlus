#pragma once

#include <QMainWindow>
#include <QListWidgetItem>
#include <QEvent>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QMessageBox>
#include <memory>
#include "Interfaz/ISystemKey.h"

QT_BEGIN_NAMESPACE
namespace Ui { class KeySequenceWindow; }
QT_END_NAMESPACE

class KeySequenceWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit KeySequenceWindow(std::shared_ptr<ISystemKey> systemKey, QWidget* parent = nullptr);
    ~KeySequenceWindow() override;

public slots:
    void toggleVisibility();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void setupConnections();
    void applyStyleSheet();
    void loadDataAsync();
    void renderShortcutItem(const ShortcutItem& item);
    void onAddClicked();
    void onDeleteClicked(const QString& rawKey, QListWidgetItem* item);

    Ui::KeySequenceWindow* ui;
    std::shared_ptr<ISystemKey> m_systemKey;
    QString m_currentRawKey;
    bool m_isLoading{ true };

    QTimer* m_delayTimer{ nullptr };
    QTimer* m_rateTimer{ nullptr };
};