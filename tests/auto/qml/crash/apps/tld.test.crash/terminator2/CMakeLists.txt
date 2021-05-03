# Generated from terminator2.pro.

#####################################################################
## terminator2plugin Generic Library:
#####################################################################

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

#### Keys ignored in scope 1:.:.:terminator2.pro:<TRUE>:
# INSTALLS = "target" "qmldir"
# OTHER_FILES = "qmldir"
# QMAKE_POST_LINK = "$$QMAKE_COPY" "$$replace($$list $$quote $$PWD/qmldir $$DESTDIR , /, $$QMAKE_DIR_SEP)"
# TEMPLATE = "lib"
# qmldir.files = "$$PWD/qmldir"
# qmldir.path = "$$DESTDIR"
# target.path = "$$DESTDIR"

qt_autogen_tools_initial_setup(terminator2plugin)
