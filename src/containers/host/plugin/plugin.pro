
include($$BASE_PRI)

TEMPLATE = lib
DESTDIR  = $$BUILD_DIR/lib/appman
TARGET   = host-container
CONFIG  += plugin

QT *= core

osx:LIBS += -undefined dynamic_lookup
win32:LIBS += $$BUILD_DIR/bin/appman.lib
android:QMAKE_LFLAGS_PLUGIN ~= s/-Wl,--no-undefined//

SOURCES += \
    hostcontainer.cpp

HEADERS += \
    hostcontainer.h

INCLUDEPATH *= ../../../common-lib
INCLUDEPATH *= ../../../manager-lib

target.path = $$INSTALL_PREFIX/lib/appman
INSTALLS += target
