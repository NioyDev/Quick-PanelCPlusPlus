#include "MainWindow.h"
#include "ui_mainwindow.h"

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

	// Ajustar la geometría de la ventana principal al 95% del ancho y 5% del alto de la pantalla
    this->setupPanelGeometry(this, 0.95, 0.05);
    // Inicializar el módulo emergente de reproducción
    m_compatWindow = std::make_unique<CompatWindow>(m_systemControl, this);
}

MainWindow::~MainWindow() {
    delete ui;
}


void MainWindow::setupPanelGeometry(QWidget* widget, double widthRatio, double heightRatio) {
    if (!widget) return;

    // Obtener la pantalla donde se encuentra actualmente el widget (soporta multimonitor)
    QScreen* screen = widget->screen();
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) return;

    QRect screenGeo = screen->geometry();

    // Calcular dimensiones basadas en porcentaje
    int panelWidth = static_cast<int>(screenGeo.width() * widthRatio);
    int panelHeight = static_cast<int>(screenGeo.height() * heightRatio);

    // Centrar horizontalmente y pegar al borde inferior (estilo barra de tareas)
    int posX = (screenGeo.width() - panelWidth) / 2; // O 0 si quieres que abarque desde la izquierda
    int posY = screenGeo.height() - panelHeight;

    // Ajustar con el offset de la pantalla por si está en un segundo monitor a la derecha/abajo
    posX += screenGeo.x();
    posY += screenGeo.y();

    widget->setGeometry(posX, posY, panelWidth, panelHeight);
}