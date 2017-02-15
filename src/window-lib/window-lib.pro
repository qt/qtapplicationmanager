TEMPLATE = lib
TARGET = QtAppManWindow
MODULE = appman_window

load(am-config)

QT = core network qml core-private
!headless:QT *= gui quick
qtHaveModule(dbus):QT *= dbus
QT_FOR_PRIVATE *= \
    appman_common-private \
    appman_application-private \
    appman_manager-private \

CONFIG *= static internal_module

multi-process:!headless {
    qtHaveModule(waylandcompositor) {
        QT *= waylandcompositor waylandcompositor-private
        HEADERS += waylandcompositor.h
        SOURCES += waylandcompositor.cpp
        PKGCONFIG += wayland-server
    } else:qtHaveModule(compositor) {
        QT *= compositor
        HEADERS += waylandcompositor-old.h
        SOURCES += waylandcompositor-old.cpp
    }
}

!headless:HEADERS += \
    window.h \
    inprocesswindow.h \
    waylandwindow.h \
    windowmanager.h \
    windowmanager_p.h \

!headless:SOURCES += \
    window.cpp \
    inprocesswindow.cpp \
    waylandwindow.cpp \
    windowmanager.cpp \

load(qt_module)
