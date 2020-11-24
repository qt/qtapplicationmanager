TEMPLATE = app
TARGET   = appman-launcher-qml

load(am-config)

QT = qml dbus core-private quick gui gui-private quick-private
QT *= \
    appman_common-private \
    appman_monitor-private \
    appman_notification-private \
    appman_application-private \
    appman_plugininterfaces-private \
    appman_launcher-private \
    appman_shared_main-private \

HEADERS += \
    launcher-qml_p.h

SOURCES += \
    launcher-qml.cpp \

load(qt_tool)

load(install-prefix)
