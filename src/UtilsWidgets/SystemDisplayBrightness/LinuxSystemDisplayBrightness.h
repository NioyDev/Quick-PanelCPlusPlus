#pragma once

#include "Interfaz/ISystemDisplayBrightness.h"

// Implementación Linux: delega en la herramienta externa `brightnessctl`.
class LinuxSystemDisplayBrightness : public ISystemDisplayBrightness {
public:
    LinuxSystemDisplayBrightness() = default;
    ~LinuxSystemDisplayBrightness() override = default;

    bool isAvailable() const override;
    std::optional<int> brightnessPercent() const override;
    bool setBrightnessPercent(int percent) override;
};