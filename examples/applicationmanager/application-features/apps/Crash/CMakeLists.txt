cmake_minimum_required(VERSION 3.16)
project(Crash LANGUAGES CXX)

find_package(Qt6 COMPONENTS Quick)
qt_policy(SET QTP0001 NEW)   # "qt/qml/" default resource prefix
qt_standard_project_setup()

qt6_add_qml_module(
    crashmodule
    VERSION 1.0
    URI "Crash"
    NO_PLUGIN
    SOURCES terminator1.cpp
    QML_FILES crashapp.qml
)

set(QML_IMPORT_PATH ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/.. CACHE STRING "" FORCE)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
    set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_DIR "${INSTALL_EXAMPLESDIR}/applicationmanager/application-features/apps/Crash")

install(
    TARGETS crashmodule
    LIBRARY DESTINATION "${INSTALL_DIR}"
    RUNTIME DESTINATION "${INSTALL_DIR}"
    BUNDLE DESTINATION "${INSTALL_DIR}"
)

add_subdirectory(Sequel)
