#pragma once

#include "Interfaz/ISystemDisplayBrightness.h"
#include <memory>

#ifdef _WIN32
#include "UtilsWidgets/SystemDisplayBrightness/WinSystemDisplayBrightness.h"
#else
#include "UtilsWidgets/SystemDisplayBrightness/LinuxSystemDisplayBrightness.h"
#endif

class SystemDisplayBrightnessFactory {
public:
    static std::shared_ptr<ISystemDisplayBrightness> create() {
#ifdef _WIN32
        return std::make_shared<WinSystemDisplayBrightness>();
#else
        return std::make_shared<LinuxSystemDisplayBrightness>();
#endif
    }
};