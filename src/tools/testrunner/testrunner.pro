include(../appman/appman.pro)

TARGET   = appman-qmltestrunner

DEFINES += AM_TESTRUNNER

CONFIG *= console

QT += qmltest qmltest-private

HEADERS += \
    testrunner.h

SOURCES += \
    testrunner.cpp

# For android installing the binaries doesn't make sense, as it's a command-line utility which doesn't work on android anyway.
android: INSTALLS=
