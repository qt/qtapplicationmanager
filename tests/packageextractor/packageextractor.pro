TARGET = tst_packageextractor

include($$PWD/../tests.pri)

QT *= \
    appman_common-private \
    appman_application-private \
    appman_package-private

SOURCES += tst_packageextractor.cpp
