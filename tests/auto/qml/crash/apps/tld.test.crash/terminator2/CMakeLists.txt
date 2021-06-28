
qt_internal_add_cmake_library(terminator2plugin
    MODULE
    INSTALL_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Terminator"
    EXCEPTIONS
    OUTPUT_DIRECTORY "$$_PRO_FILE_PWD_/Terminator"
    SOURCES
        qmlterminator2.cpp qmlterminator2.h
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
        Qt::Qml
        Qt::Quick
)

qt_autogen_tools_initial_setup(terminator2plugin)
