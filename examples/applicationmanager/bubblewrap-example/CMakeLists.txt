
cmake_minimum_required(VERSION 3.16)
project(bubblewrap-example LANGUAGES CXX)

qt_policy(SET QTP0001 NEW)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/bubblewrap-example")

find_package(Qt6 COMPONENTS Core)
find_package(Qt6 COMPONENTS Gui)
find_package(Qt6 COMPONENTS AppManMainPrivate)

qt_am_add_systemui_wrapper(bubblewrap-example
    INSTALL_DIR "${INSTALL_EXAMPLEDIR}"
    EXTRA_FILES apps system-ui.qml bubblewrap-network-setup.sh
    CONFIG_YAML am-config.yaml
    EXTRA_ARGS --verbose
)

qt_add_qml_module(networkhelper
    URI "NetworkHelper"
    SOURCES
        networkhelper.cpp networkhelper.h
    PLUGIN_TARGET networkhelper
    OUTPUT_DIRECTORY
        imports/NetworkHelper
)
