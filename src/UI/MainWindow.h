#pragma once

#include <QMainWindow>
#include <memory>
#include "Interfaz/ISystemControl.h"
#include "Interfaz/ISystemKey.h"
#include "CompatWindow.h"
#include "KeySequenceWindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(std::shared_ptr<ISystemControl> systemControl, std::shared_ptr<ISystemKey> systemKey, QWidget* parent = nullptr);
    ~MainWindow() override;

private:

    void setupPanelGeometry(QWidget* widget, double widthRatio = 0.90, double heightRatio = 0.05);

    Ui::MainWindow* ui;
    std::shared_ptr<ISystemControl> m_systemControl;
    std::shared_ptr<ISystemKey> m_systemKey;
    std::unique_ptr<CompatWindow> m_compatWindow;
    std::unique_ptr<KeySequenceWindow> m_keySequenceWindow;
};