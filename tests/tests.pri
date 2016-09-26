load(am-config)

CONFIG *= console testcase
QT = core network testlib
qtHaveModule(dbus):QT *= dbus

DEFINES *= AM_TESTDATA_DIR=\\\"$$PWD/data/\\\"

HEADERS += \
    $$PWD/error-checking.h \
    $$PWD/sudo-cleanup.h \
