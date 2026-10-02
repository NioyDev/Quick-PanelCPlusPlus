#include <iostream>
#include <QApplication>
#include "UI/MainWindow.h"
#include "Factorys/SystemControlFactory.h"
#include "Factorys/SystemKeyFactory.h"

int main(int argc, char **argv) {


    
    std::cout << "Inicio Quick Panel\n";
    QApplication app(argc, argv);

    // Crear la instancia adecuada (Windows o Linux) vía Factory
    auto systemControl = SystemControlFactory::create();
    auto systemKey = SystemKeyFactory::create();

    // Inyectar el servicio al MainWindow
    MainWindow w(systemControl, systemKey);
    w.show();

    return app.exec();
}
