#pragma once

#include "Interfaz/ISystemControl.h"
#include <memory>

#ifdef _WIN32
#include "Utils&Widgets/SystemControl/WinSystemControl.h"
#else
#include "Utils&Widgets/SystemControl/LinuxSystemControl.h"
#endif

class SystemControlFactory {
public:
    static std::shared_ptr<ISystemControl> create() {
#ifdef _WIN32
        return std::make_shared<WinSystemControl>();
#else
        return std::make_shared<LinuxSystemControl>();
#endif
    }
};