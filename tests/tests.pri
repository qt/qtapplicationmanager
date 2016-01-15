
TEMPLATE = app
DESTDIR  = $$BUILD_DIR/bin

load(am-config)

CONFIG *= console testcase
QT = core network testlib
qtHaveModule(dbus):QT *= dbus

DEFINES *= AM_BUILD_APPMAN
DEFINES *= AM_TESTDATA_DIR=\\\"$$PWD/data/\\\"

target.path = $$INSTALL_PREFIX/bin/
INSTALLS += target

HEADERS += \
    $$PWD/error-checking.h \
    $$PWD/sudo-cleanup.h \
