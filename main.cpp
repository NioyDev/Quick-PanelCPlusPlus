#include <QApplication>
#include <QPushButton>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QPushButton boton("¡Hola, Mundo con Qt!");
    boton.resize(250, 70);
    boton.show();

    return app.exec(); // Inicia el bucle de eventos de la aplicación
}
