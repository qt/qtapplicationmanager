TEMPLATE = lib
TARGET = QtAppManWindow
MODULE = appman_window

load(am-config)

QT = core network qml core-private
!headless:QT *= gui quick
QT_FOR_PRIVATE *= \
    appman_common-private \
    appman_application-private \
    appman_manager-private \

CONFIG *= static internal_module

multi-process:!headless {
    HEADERS += \
        waylandcompositor.h \
        waylandwindow.h \
        waylandqtamserverextension_p.h

    SOURCES += \
        waylandcompositor.cpp \
        waylandwindow.cpp \
        waylandqtamserverextension.cpp

    qtHaveModule(waylandcompositor) {
        QT *= waylandcompositor
        # Qt < 5.14 is missing the sendPopupDone() method in QWaylandXdgShell
        !versionAtLeast(QT_VERSION, 5.14.0):QT *= waylandcompositor-private
    }
    WAYLANDSERVERSOURCES += \
        ../wayland-extensions/qtam-extension.xml

    CONFIG *= wayland-scanner generated_privates
    private_headers.CONFIG += no_check_exists
}

!headless:HEADERS += \
    window.h \
    windowitem.h \
    inprocesswindow.h \
    windowmanager.h \
    windowmanager_p.h \
    touchemulation.h \


!headless:SOURCES += \
    window.cpp \
    windowitem.cpp \
    inprocesswindow.cpp \
    windowmanager.cpp \
    touchemulation.cpp \

# build the touch emulation only on X11 setups
!headless:config_touchemulation {
    PKGCONFIG *= xcb x11 xi
    QT *= gui-private testlib
    HEADERS += touchemulation_x11_p.h
    SOURCES += touchemulation_x11.cpp
}

load(qt_module)
