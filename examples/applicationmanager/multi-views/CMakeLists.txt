
cmake_minimum_required(VERSION 3.16)
project(multi-views LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/multi-views")

find_package(Qt6 COMPONENTS Core)
find_package(Qt6 COMPONENTS Gui)
find_package(Qt6 COMPONENTS AppManMainPrivate)

qt6_am_add_systemui_wrapper(multi-views
    INSTALL_DIR "${INSTALL_EXAMPLEDIR}"
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps system-ui
    EXTRA_ARGS --verbose
)
