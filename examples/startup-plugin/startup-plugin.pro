
TEMPLATE = lib
CONFIG += plugin c++11
TARGET = startup-plugin

QT = core appman_plugininterfaces-private

HEADERS += \
    startup-plugin.h

SOURCES += \
    startup-plugin.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/startup-plugin
INSTALLS += target
