
cmake_minimum_required(VERSION 3.16)
project(intents LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/intents")

find_package(Qt6 COMPONENTS Core)
find_package(Qt6 COMPONENTS Gui)
find_package(Qt6 COMPONENTS AppManMainPrivate)

qt6_am_add_systemui_wrapper(intents
    MAIN_QML_FILE system-ui.qml
    EXTRA_FILES apps shared
    EXTRA_ARGS --builtin-apps-manifest-dir apps
)
