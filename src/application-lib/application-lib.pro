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
    packagescanner.h \
    yamlpackagescanner.h \
    installationreport.h \
    applicationinterface.h \
    intentinfo.h \
    packageinfo.h \
    packagedatabase.h \

SOURCES += \
    applicationinfo.cpp \
    yamlpackagescanner.cpp \
    installationreport.cpp \
    applicationinterface.cpp \
    intentinfo.cpp \
    packageinfo.cpp \
    packagedatabase.cpp \

load(qt_module)
