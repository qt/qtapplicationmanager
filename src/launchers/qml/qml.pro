TEMPLATE = app
TARGET   = appman-launcher-qml

load(am-config)

QT = qml dbus core-private
!headless:QT += quick gui gui-private quick-private
QT *= \
    appman_common-private \
    appman_application-private \
    appman_notification-private \

SOURCES += \
    main.cpp \
    qmlapplicationinterface.cpp \
    ipcwrapperobject.cpp

!headless:SOURCES += \
    applicationmanagerwindow.cpp \

HEADERS += \
    qmlapplicationinterface.h \
    $$SOURCE_DIR/src/manager-lib/applicationinterface.h \
    ipcwrapperobject.h \
    ipcwrapperobject_p.h

!headless:HEADERS += \
    applicationmanagerwindow.h \

load(qt_tool)
