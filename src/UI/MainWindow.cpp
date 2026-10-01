#include "MainWindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(std::shared_ptr<ISystemControl> systemControl, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_systemControl(std::move(systemControl)) {
    ui->setupUi(this);

    // Inicializar el módulo emergente de reproducción
    m_compatWindow = std::make_unique<CompatWindow>(m_systemControl, this);
}

MainWindow::~MainWindow() {
    delete ui;
}