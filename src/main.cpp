#include <iostream>
#include <QApplication>
#include "UI/MainWindow.h"
#include "Factorys/SystemControlFactory.h"
#include "Factorys/SystemKeyFactory.h"
#include "Factorys/SystemBatteryFactory.h"
#include "Factorys/SystemBluetoothControlFactory.h"

int main(int argc, char **argv) {


    
    std::cout << "Inicio Quick Panel\n";
    QApplication app(argc, argv);

    // Crear la instancia adecuada (Windows o Linux) vía Factory
    auto systemControl = SystemControlFactory::create();
    auto systemKey = SystemKeyFactory::create();
    auto systemBattery = ControlBatteryFactory::create();
    auto systemBluetooth = SystemBluetoothControlFactory::create();

    // Inyectar el servicio al MainWindow
    MainWindow w(systemControl, systemKey, systemBattery, systemBluetooth);
    w.show();

    return app.exec();
}
