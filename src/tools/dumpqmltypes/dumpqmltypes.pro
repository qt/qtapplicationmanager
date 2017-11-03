requires(!headless)

TEMPLATE = app
TARGET   = appman-dumpqmltypes

load(am-config)

QT = core
QT *= \
    appman_common-private \
    appman_application-private \
    appman_manager-private \
    appman_installer-private \
    appman_notification-private \
    appman_window-private \
    appman_launcher-private \

CONFIG *= console

SOURCES += \
    dumpqmltypes.cpp

load(qt_tool)

load(install-prefix)
