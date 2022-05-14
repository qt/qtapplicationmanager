
qt_internal_add_cmake_library(qmlcrashplugin
    MODULE
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/QmlCrash"
    SOURCES
        ../../qmlcrash/qmlcrash.cpp ../../qmlcrash/qmlcrash.h
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::Qml
)

qt_internal_set_exceptions_flags(qmlcrashplugin TRUE)

qt_autogen_tools_initial_setup(qmlcrashplugin)
