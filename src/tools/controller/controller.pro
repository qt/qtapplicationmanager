
TEMPLATE = app
TARGET   = appman-controller
DESTDIR  = $$BUILD_DIR/bin

include($$BASE_PRI)

CONFIG *= console
QT = core network dbus

DEFINES *= AM_BUILD_APPMAN

target.path = $$INSTALL_PREFIX/bin/
INSTALLS += target

SOURCES +=  \
    main.cpp \

DBUS_INTERFACES += \
    ../../dbus/io.qt.applicationinstaller.xml \
    ../../dbus/io.qt.applicationmanager.xml
