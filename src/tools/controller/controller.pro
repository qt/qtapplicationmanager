
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

DBUS_INTERFACES += \
    ../../dbus/io.qt.applicationinstaller.xml \
    ../../dbus/io.qt.applicationmanager.xml

OTHER_FILES += \
    controller.qdoc \
