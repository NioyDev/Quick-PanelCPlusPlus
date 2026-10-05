#pragma once

#include "Interfaz/ISystemControl.h"
#include <memory>

#ifdef _WIN32
#include "UtilsWidgets/SystemControl/WinSystemControl.h"
#else
#include "UtilsWidgets/SystemControl/LinuxSystemControl.h"
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