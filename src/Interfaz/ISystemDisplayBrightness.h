#pragma once

#include <optional>

/**
 * Contrato para leer/modificar el brillo de la pantalla principal.
 * Cada plataforma (Windows / Linux) implementa su propia estrategia.
 */
class ISystemDisplayBrightness {
public:
    virtual ~ISystemDisplayBrightness() = default;

    // true si la plataforma puede controlar el brillo (herramienta/driver presente).
    virtual bool isAvailable() const = 0;

    // Brillo actual en porcentaje [0..100]. std::nullopt si no se pudo leer.
    virtual std::optional<int> brightnessPercent() const = 0;

    // Aplica el brillo en porcentaje [0..100] (se recorta si viene fuera de rango).
    virtual bool setBrightnessPercent(int percent) = 0;
};