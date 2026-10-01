#pragma once

#include <QMainWindow>
#include <memory>
#include "Interfaz/ISystemControl.h"
#include "CompatWindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(std::shared_ptr<ISystemControl> systemControl, QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow* ui;
    std::shared_ptr<ISystemControl> m_systemControl;
    std::unique_ptr<CompatWindow> m_compatWindow;
};