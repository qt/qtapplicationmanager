include(../appman/appman.pro)

requires(!headless)

TARGET   = appman-qmltestrunner

DEFINES += AM_TESTRUNNER

CONFIG *= console

QT += qmltest qmltest-private

HEADERS += \
    testrunner.h \
    testrunner_p.h

SOURCES += \
    testrunner.cpp

# For android installing the binaries doesn't make sense, as it's a command-line utility which doesn't work on android anyway.
android: INSTALLS=
