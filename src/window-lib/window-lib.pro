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
    HEADERS += \
        waylandcompositor.h \
        waylandwindow.h

    SOURCES += \
        waylandcompositor.cpp \
        waylandwindow.cpp

    qtHaveModule(waylandcompositor):qtHaveModule(waylandcompositor-private) {
        QT *= waylandcompositor waylandcompositor-private
        !osx:PKGCONFIG += wayland-server

        HEADERS += waylandcompositor_p.h
    } else:qtHaveModule(compositor) {
        QT *= compositor
    }
}

!headless:HEADERS += \
    window.h \
    inprocesswindow.h \
    windowmanager.h \
    windowmanager_p.h \

!headless:SOURCES += \
    window.cpp \
    inprocesswindow.cpp \
    windowmanager.cpp \

load(qt_module)

