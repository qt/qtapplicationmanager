TEMPLATE = lib
TARGET = QtAppManApplication
MODULE = appman_application

load(am-config)

QT = core network
QT_FOR_PRIVATE *= \
    appman_common-private \

CONFIG *= static internal_module
CONFIG -= create_cmake

HEADERS += \
    applicationinfo.h \
    applicationscanner.h \
    yamlapplicationscanner.h \
    installationreport.h \
    applicationinterface.h \

SOURCES += \
    applicationinfo.cpp \
    yamlapplicationscanner.cpp \
    installationreport.cpp \
    applicationinterface.cpp \

load(qt_module)
