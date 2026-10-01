#include "DisplayUtils.h"
#include <QScreen>
#include <QGuiApplication>

namespace utils {
    void setupPanelGeometry(QWidget* widget, double widthRatio, double heightRatio) {
        if (!widget) return;

        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            int screenWidth = screen->geometry().width();
            int screenHeight = screen->geometry().height();

            // Calcular tamaño basado en el porcentaje
            int panelWidth = static_cast<int>(screenWidth * widthRatio);
            int panelHeight = static_cast<int>(screenHeight * heightRatio);

            // Calcular posición para que quede centrado
            int posX = (screenWidth - panelWidth) / 2;
            int posY = (screenHeight - panelHeight) / 2;

            // Aplicar la nueva geometría
            widget->setGeometry(posX, posY, panelWidth, panelHeight);
        }
    }
}
