cmake_minimum_required(VERSION 3.16)

if(WIN32)
    set(Qt6_DIR "C:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6")
endif()

project(QtSimpleApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

# NO MODIFICAR : Se requiere Qt 6.8 o superior
if(WIN32)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.8.3/msvc2022_64")
    find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Concurrent)
elseif(UNIX AND NOT APPLE)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Concurrent)
else()
    message(FATAL_ERROR "Sistema operativo no soportado.")
endif()

set(SOURCES
    src/main.cpp
    src/Factorys/SystemControlFactory.cpp
    src/UI/MainWindow.cpp
    src/UI/CompatWindow.cpp
    src/UI/KeySequenceWindow.cpp
    ui/mainwindow.ui
    ui/compatwindow.ui
    ui/keysequencewindow.ui
)

set(HEADERS
    src/Interfaz/ISystemControl.h
    src/Interfaz/ISystemKey.h
    src/Factorys/SystemControlFactory.h
    src/UI/MainWindow.h
    src/UI/CompatWindow.h
    src/UI/KeySequenceWindow.h
)

if(WIN32)
    list(APPEND SOURCES src/utils/WinSystemControl.cpp)
    list(APPEND HEADERS src/utils/WinSystemControl.h)
    list(APPEND HEADERS src/utils/WinSystemKey.h)
    list(APPEND SOURCES src/utils/WinSystemKey.cpp)
    add_compile_definitions(
        _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
else()
    list(APPEND SOURCES src/utils/LinuxSystemControl.cpp)
    list(APPEND HEADERS src/utils/LinuxSystemControl.h)
    list(APPEND HEADERS src/utils/LinuxSystemKey.h)
    list(APPEND SOURCES src/utils/LinuxSystemKey.cpp)
endif()

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
 "src/Interfaz/ISystemBattery.h"  "src/utils/LinuxSystemBattery.h" "src/utils/LinuxSystemBattery.cpp" "src/utils/WinSystemBattery.h" "src/utils/WinSystemBattery.cpp" "src/Factorys/SystemBatteryFactory.h" "src/Factorys/SystemBatteryFactory.cpp" "src/UI/QuickBatteryWindow.h" "src/UI/QuickBatteryWindow.cpp")

target_include_directories(${PROJECT_NAME} PRIVATE 
    src
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Widgets
        Qt6::Concurrent
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE WindowsApp
    Setupapi
      PowrProf)

    # Obtener ruta binaria de Qt directamente desde Qt6::Core
    get_target_property(QT_CORE_LIB_PATH Qt6::Core LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_CORE_LIB_PATH}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
    
    
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E env "PATH=${QT_BIN_DIR};$ENV{PATH}"
                    ${WINDEPLOYQT_EXECUTABLE}
                    $<$<CONFIG:Debug>:--debug>
                    $<$<CONFIG:Release>:--release>
                    --no-compiler-runtime
                    $<TARGET_FILE:${PROJECT_NAME}>
            COMMENT "Ejecutando windeployqt automáticamente para copiar DLLs y plugins..."
        )
    endif()
endif()