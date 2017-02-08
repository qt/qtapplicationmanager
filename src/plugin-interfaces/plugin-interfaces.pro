TEMPLATE = lib
TARGET = QtAppManPluginInterfaces
MODULE = appman_plugininterfaces
QT.appman_plugininterfaces.name = AppManPluginInterfaces

QT = core
CONFIG *= static internal_module create_cmake

CMAKE_MODULE_TESTS = '-'

HEADERS = \
    startupinterface.h \
    containerinterface.h

load(qt_module)

SOURCES += \
    startupinterface.cpp \
    containerinterface.cpp

