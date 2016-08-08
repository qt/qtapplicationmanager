
TEMPLATE = app
TARGET   = appman-controller
DESTDIR  = $$BUILD_DIR/bin

load(am-config)

CONFIG *= console
QT = core network dbus

DEFINES *= AM_BUILD_APPMAN

load(add-static-library)
addStaticLibrary(../../common-lib)

target.path = $$INSTALL_PREFIX/bin/
INSTALLS += target

SOURCES +=  \
    main.cpp \

appmanif.files =  ../../dbus/io.qt.applicationmanager.xml
appmanif.header_flags = -i dbus-utilities.h

DBUS_INTERFACES += \
    ../../dbus/io.qt.applicationinstaller.xml \
    appmanif



OTHER_FILES += \
    controller.qdoc \
