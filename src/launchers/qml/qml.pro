
TEMPLATE = app
TARGET   = appman-launcher-qml
DESTDIR  = $$BUILD_DIR/bin

include($$BASE_PRI)

QT = qml dbus core-private
!headless:QT += quick gui gui-private

SOURCES += \
    main.cpp \
    qmlapplicationinterface.cpp \

!headless:SOURCES += \
    applicationmanagerwindow.cpp \

HEADERS += \
    qmlapplicationinterface.h \
    $$SOURCE_DIR/src/manager-lib/applicationinterface.h \

!headless:HEADERS += \
    applicationmanagerwindow.h \

addStaticLibrary(../../common-lib)
addStaticLibrary(../../notification-lib)

target.path = $$INSTALL_PREFIX/bin/
INSTALLS += target
