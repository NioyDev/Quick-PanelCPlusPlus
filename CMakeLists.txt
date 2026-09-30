cmake_minimum_required(VERSION 3.16)

project(QtSimpleApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Habilitar la generación automática de archivos Meta-Object Compiler (MOC), User Interface (UIC) y Resource (RCC)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Configuración condicional de versiones de Qt según la plataforma
if(WIN32)
    # En Windows requiere exactamente la serie Qt 6.5
    find_package(Qt6 6.5 EXACT REQUIRED COMPONENTS Widgets)
elseif(UNIX AND NOT APPLE)
    # En Linux requiere cualquier versión de Qt 6
    find_package(Qt6 6.0 REQUIRED COMPONENTS Widgets)
else()
    message(FATAL_ERROR "Sistema operativo no soportado.")
endif()

# Definición del ejecutable
add_executable(${PROJECT_NAME}
    main.cpp
)

# Enlace de las bibliotecas de Qt con el ejecutable
target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
        Qt6::Widgets
)