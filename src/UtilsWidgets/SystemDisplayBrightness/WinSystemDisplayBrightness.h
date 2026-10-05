#pragma once

#include "Interfaz/ISystemDisplayBrightness.h"


class WinSystemDisplayBrightness : public ISystemDisplayBrightness {
public:
    WinSystemDisplayBrightness() = default;
    ~WinSystemDisplayBrightness() override = default;

    bool isAvailable() const override;
    std::optional<int> brightnessPercent() const override;
    bool setBrightnessPercent(int percent) override;
};