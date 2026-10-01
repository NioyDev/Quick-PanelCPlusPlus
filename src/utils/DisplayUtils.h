#pragma once
#include <QWidget>

namespace utils {
    /**
     * Ajusta la geometría de un QWidget basado en un porcentaje de la pantalla
     * y lo centra automáticamente.
     * 
     * @param widget El widget o ventana a redimensionar.
     * @param widthRatio Proporción del ancho de la pantalla (ej. 0.90 = 90%)
     * @param heightRatio Proporción del alto de la pantalla (ej. 0.20 = 20%)
     */
    void setupPanelGeometry(QWidget* widget, double widthRatio = 0.90, double heightRatio = 0.05);
}

