TEMPLATE = app
TARGET   = appman-qmltestrunner

DEFINES += AM_TESTRUNNER

include($$PWD/../../manager/manager.pri)

QT += qmltest qmltest-private

HEADERS += \
    testrunner.h

SOURCES += \
    testrunner.cpp
