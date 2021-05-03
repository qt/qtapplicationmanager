load(am-config)

CONFIG *= console testcase
QT = core network testlib
qtHaveModule(dbus):QT *= dbus

DEFINES *= AM_TESTDATA_DIR=\\\"$$PWD/data/\\\"
DEFINES -= QT_NO_CAST_FROM_ASCII

HEADERS += \
    $$PWD/error-checking.h \
