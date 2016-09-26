TEMPLATE = app
TARGET   = appman-controller

load(am-config)

QT = core network dbus
QT *= appman_common-private

CONFIG *= console

SOURCES +=  \
    main.cpp \

appmanif.files =  ../../dbus/io.qt.applicationmanager.xml
appmanif.header_flags = -i dbus-utilities.h

DBUS_INTERFACES += \
    ../../dbus/io.qt.applicationinstaller.xml \
    appmanif

OTHER_FILES += \
    controller.qdoc \

load(qt_tool)
