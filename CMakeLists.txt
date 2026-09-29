# Copyright (C) 2026 the eleven-kde authors (AI-assisted, see README.md)
# SPDX-License-Identifier: LGPL-3.0-only
cmake_minimum_required(VERSION 3.21)
project(eleven-kde VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The style uses Qt private headers/API, so it is tied to the exact Qt build it was compiled against.
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets WidgetsPrivate GuiPrivate CorePrivate)

add_library(eleven-kde MODULE
    src/main.cpp
    src/elevenstyle.cpp
    src/elevenstyle.h
)
target_link_libraries(eleven-kde PRIVATE Qt6::Widgets Qt6::WidgetsPrivate Qt6::GuiPrivate Qt6::CorePrivate)
set_target_properties(eleven-kde PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/styles
    PREFIX "")

# Where Qt looks for style plugins. Default: the system Qt's plugin directory (installing there needs root).
# Override with -DELEVEN_PLUGIN_DIR=/some/dir if you set QT_PLUGIN_PATH yourself.
find_program(ELEVEN_QMAKE NAMES qmake6 qmake)
if(ELEVEN_QMAKE)
    execute_process(COMMAND ${ELEVEN_QMAKE} -query QT_INSTALL_PLUGINS
                    OUTPUT_VARIABLE ELEVEN_QT_PLUGINS OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
set(ELEVEN_PLUGIN_DIR "${ELEVEN_QT_PLUGINS}" CACHE PATH "Qt plugin directory to install the style into")
if(ELEVEN_PLUGIN_DIR)
    install(TARGETS eleven-kde LIBRARY DESTINATION ${ELEVEN_PLUGIN_DIR}/styles)
endif()

# ---- developer tools (not installed) ----
option(ELEVEN_BUILD_TOOLS "Build the widget gallery and QML test window" ON)
if(ELEVEN_BUILD_TOOLS)
    add_executable(gallery tools/gallery.cpp)
    target_link_libraries(gallery PRIVATE Qt6::Widgets)
    find_package(KF6WidgetsAddons QUIET)
    if(KF6WidgetsAddons_FOUND)
        target_link_libraries(gallery PRIVATE KF6::WidgetsAddons)
        target_compile_definitions(gallery PRIVATE GALLERY_HAS_KF)
    endif()

    find_package(Qt6 QUIET COMPONENTS Quick Qml)
    if(Qt6Quick_FOUND)
        add_executable(qmltest tools/qmltest.cpp)
        target_link_libraries(qmltest PRIVATE Qt6::Widgets Qt6::Quick Qt6::Qml)
    endif()
endif()
