#pragma once

#include "Interfaz/ISystemKey.h"
#include <memory>

#ifdef _WIN32
#include "Utils&Widgets/SystemKey/WinSystemKey.h"
#else
#include "Utils&Widgets/SystemKey/LinuxSystemKey.h"
#endif

class SystemKeyFactory {
public:
    static std::shared_ptr<ISystemKey> create() {
#ifdef _WIN32
        return std::make_shared<WinSystemKey>();
#else
        return std::make_shared<LinuxSystemKey>();
#endif
    }
};