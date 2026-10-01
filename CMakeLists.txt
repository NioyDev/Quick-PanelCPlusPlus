cmake_minimum_required(VERSION 3.16)

if(WIN32)
    set(Qt6_DIR "C:/Qt/6.8.3/msvc2022_64/lib/cmake/Qt6")
endif()

project(QtSimpleApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

if(WIN32)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.8.3/msvc2022_64")
    find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
elseif(UNIX AND NOT APPLE)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
else()
    message(FATAL_ERROR "Sistema operativo no soportado.")
endif()

set(SOURCES
    src/main.cpp
    src/Factorys/SystemControlFactory.cpp
    src/UI/MainWindow.cpp
    src/UI/CompatWindow.cpp
    ui/mainwindow.ui
    ui/compatwindow.ui
)

set(HEADERS
    src/Interfaz/ISystemControl.h
    src/Factorys/SystemControlFactory.h
    src/UI/MainWindow.h
    src/UI/CompatWindow.h
)

if(WIN32)
    list(APPEND SOURCES src/utils/WinSystemControl.cpp)
    list(APPEND HEADERS src/utils/WinSystemControl.h)
    add_compile_definitions(
        _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
else()
    list(APPEND SOURCES src/utils/LinuxSystemControl.cpp)
    list(APPEND HEADERS src/utils/LinuxSystemControl.h)
endif()

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /permissive-)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE 
    src
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Widgets
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE WindowsApp)

    # 💡 OBTENER RUTA BINARIA DE QT DIRECTAMENTE DESDE Qt6::Core
    get_target_property(QT_CORE_LIB_PATH Qt6::Core LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_CORE_LIB_PATH}" DIRECTORY)

    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            # Pasa QT_BIN_DIR en el PATH para que windeployqt pueda cargar Qt6Core.dll
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