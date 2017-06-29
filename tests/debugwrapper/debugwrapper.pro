TARGET = tst_debugwrapper

include($$PWD/../tests.pri)

QT *= \
    appman_manager-private

SOURCES += tst_debugwrapper.cpp
