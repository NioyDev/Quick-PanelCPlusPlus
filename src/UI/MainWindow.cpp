#include "MainWindow.h"
#include "ui_mainwindow.h"
#include "utils/DisplayUtils.h"

MainWindow::MainWindow(std::shared_ptr<ISystemControl> systemControl, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_systemControl(std::move(systemControl)) {
    ui->setupUi(this);

    // Ventana sin marco y SIEMPRE por encima de las demás aplicaciones
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // Hacer el fondo de la ventana transparente para permitir bordes redondeados
    this->setAttribute(Qt::WA_TranslucentBackground);

    // IMPORTANTE: Qt ignora los fondos CSS en widgets contenedores por defecto.
    // Esto obliga a pintar los bordes redondeados y el fondo.
    ui->centralwidget->setAttribute(Qt::WA_StyledBackground, true);

    // Aplicar estilo de bordes redondeados y un color de fondo (Gris oscuro) al widget principal
    ui->centralwidget->setStyleSheet("QWidget#centralwidget { background-color: #fcfcfc; border-radius: 90px; }");

    // Aplicar diseño de barra (95% ancho, 5% alto, centrado)
    utils::setupPanelGeometry(this, 0.95, 0.05);

    // Inicializar el módulo emergente de reproducción
    m_compatWindow = std::make_unique<CompatWindow>(m_systemControl, this);
}

MainWindow::~MainWindow() {
    delete ui;
}