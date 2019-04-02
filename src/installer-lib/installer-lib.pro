TEMPLATE = lib
TARGET = QtAppManInstaller
MODULE = appman_installer

load(am-config)

QT = core network qml
QT_FOR_PRIVATE *= \
    appman_common-private \
    appman_crypto-private \
    appman_application-private \
    appman_package-private \

CONFIG *= static internal_module
CONFIG -= create_cmake

include($$SOURCE_DIR/3rdparty/libarchive.pri)
include($$SOURCE_DIR/3rdparty/libz.pri)

HEADERS += \
    asynchronoustask.h \
    deinstallationtask.h \
    installationtask.h \
    scopeutilities.h \
    installationlocation.h \
    package.h \
    packagemanager.h \
    packagemanager_p.h \
    sudo.h \

SOURCES += \
    asynchronoustask.cpp \
    installationtask.cpp \
    deinstallationtask.cpp \
    scopeutilities.cpp \
    installationlocation.cpp \
    packagemanager.cpp \
    package.cpp \
    sudo.cpp \

load(qt_module)
