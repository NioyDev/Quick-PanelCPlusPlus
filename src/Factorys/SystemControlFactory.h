#pragma once

#include "Interfaz/ISystemControl.h"
#include <memory>

#ifdef _WIN32
#include "utils/WinSystemControl.h"
#else
#include "utils/LinuxSystemControl.h"
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