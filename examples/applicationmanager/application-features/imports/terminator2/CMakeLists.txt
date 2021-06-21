# Generated from terminator2.pro.

cmake_minimum_required(VERSION 3.14)
project(terminator2plugin LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/application-features/apps/crash/Terminator")

find_package(Qt6 COMPONENTS Core)
find_package(Qt6 COMPONENTS Gui)
find_package(Qt6 COMPONENTS Qml)
find_package(Qt6 COMPONENTS Quick)

qt6_add_qml_module(terminator2plugin
    OUTPUT_DIRECTORY "../../apps/crash/Terminator"
    VERSION 2.0
    URI "Terminator"
    NO_GENERATE_PLUGIN_SOURCE
    PLUGIN_TARGET terminator2plugin
)

target_sources(terminator2plugin PRIVATE
    qmlterminator2.cpp qmlterminator2.h
)

set_target_properties(terminator2plugin PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
target_link_libraries(terminator2plugin PUBLIC
    Qt::Core
    Qt::Gui
    Qt::Qml
    Qt::Quick
)

install(TARGETS terminator2plugin
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/../../apps/crash/Terminator/qmldir"
    DESTINATION "${INSTALL_EXAMPLEDIR}"
)
