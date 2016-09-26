TARGET = tst_packageextractor

cross_compile:CONFIG += insignificant_test # no test-data available in the CI

include($$PWD/../tests.pri)

QT *= \
    appman_common-private \
    appman_application-private \
    appman_package-private

SOURCES += tst_packageextractor.cpp
