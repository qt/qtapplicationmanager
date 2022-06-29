TEMPLATE = app
TARGET   = custom-appman

CONFIG += c++11 link_pkgconfig exceptions console
CONFIG -= app_bundle qml_debug

DEFINES += QT_MESSAGELOGCONTEXT

# This define flags us as an "appman" and allows us to link against the AppMan's private libraries
DEFINES *= AM_COMPILING_APPMAN

QT = appman_main-private

SOURCES = custom-appman.cpp

OTHER_FILES += \
    doc/src/*.qdoc \
    doc/images/*.png \

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/custom-appman
INSTALLS += target

example_sources.path = $$target.path
example_sources.files = $SOURCES doc
INSTALLS += example_sources
