include(../appman/appman.pro)

TARGET   = appman-qmltestrunner

DEFINES += AM_TESTRUNNER

CONFIG *= console

QT += qmltest qmltest-private

HEADERS += \
    testrunner.h

SOURCES += \
    testrunner.cpp
