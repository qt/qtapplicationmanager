TARGET = QtAppManInstaller
MODULE = appman_installer

load(am-config)

QT = core network qml
qtHaveModule(dbus):QT *= dbus
QT_FOR_PRIVATE *= \
    appman_common-private \
    appman_crypto-private \
    appman_application-private \
    appman_package-private \
    appman_manager-private

CONFIG *= static internal_module

include($$SOURCE_DIR/3rdparty/libarchive.pri)
include($$SOURCE_DIR/3rdparty/libz.pri)

HEADERS += \
    installationlocation.h \
    asynchronoustask.h \
    deinstallationtask.h \
    installationtask.h \
    scopeutilities.h \
    applicationinstaller.h \
    applicationinstaller_p.h \
    sudo.h \

SOURCES += \
    installationlocation.cpp \
    asynchronoustask.cpp \
    installationtask.cpp \
    deinstallationtask.cpp \
    scopeutilities.cpp \
    applicationinstaller.cpp \
    sudo.cpp \

load(qt_module)
