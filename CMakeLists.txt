cmake_minimum_required(VERSION 3.16)

if(WIN32)
    # Apunta directamente a la carpeta donde está Qt6Config.cmake
    set(Qt6_DIR "C:/Qt/6.5.3/msvc2019_64/lib/cmake/Qt6")
endif()

project(QtSimpleApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generación automática de MOC, UIC y RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Búsqueda condicional de Qt según la plataforma
if(WIN32)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.5.3/msvc2019_64")
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
elseif(UNIX AND NOT APPLE)
    find_package(Qt6 6.0 REQUIRED COMPONENTS Widgets WebEngineWidgets)
else()
    message(FATAL_ERROR "Sistema operativo no soportado.")
endif()

# Definición de archivos fuente dentro de la carpeta src/
set(SOURCES
    src/main.cpp
    # Agrega aquí más fuentes: src/mainwindow.cpp, etc.
)

# Si usas recursos de Qt (archivos .qrc para empaquetar HTML)
set(RESOURCES
    resources.qrc
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    #${RESOURCES}
)

# Indicar a CMake que busque encabezados en src/
target_include_directories(${PROJECT_NAME} PRIVATE src)

# Enlace de librerías
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Widgets
        # Qt6::WebEngineWidgets # Descomenta si usas QWebEngineView para renderizar HTML
)