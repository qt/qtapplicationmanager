
qt_internal_add_cmake_library(terminator2plugin
    MODULE
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Terminator"
    SOURCES
        ../../terminator2/qmlterminator2.cpp ../../terminator2/qmlterminator2.h
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::Qml
)

qt_internal_set_exceptions_flags(terminator2plugin TRUE)

qt_autogen_tools_initial_setup(terminator2plugin)
