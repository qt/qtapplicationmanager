
TEMPLATE = app
TARGET   = appman-launcher-qml
DESTDIR  = $$BUILD_DIR/bin

load(am-config)

QT = qml dbus core-private
!headless:QT += quick gui gui-private quick-private

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

load(add-static-library)
addStaticLibrary(../../common-lib)
addStaticLibrary(../../manager-lib)
addStaticLibrary(../../notification-lib)

target.path = $$INSTALL_PREFIX/bin/
INSTALLS += target
