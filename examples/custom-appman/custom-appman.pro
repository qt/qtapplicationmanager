TEMPLATE = app
TARGET   = custom-appman

CONFIG += c++11 link_pkgconfig exceptions console
CONFIG -= app_bundle qml_debug

DEFINES += QT_MESSAGELOGCONTEXT

QT = appman_main-private

SOURCES = custom-appman.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/custom-appman
INSTALLS += target
