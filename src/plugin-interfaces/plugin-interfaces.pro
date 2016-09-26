TARGET = QtAppManPluginInterfaces
MODULE = appman_plugininterfaces

QT = core
CONFIG *= static internal_module

HEADERS = \
    startupinterface.h \
    containerinterface.h

load(qt_module)

SOURCES += \
    startupinterface.cpp \
    containerinterface.cpp
