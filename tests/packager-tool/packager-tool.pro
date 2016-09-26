TARGET = tst_packager-tool

cross_compile:CONFIG += insignificant_test # no test-data available in the CI

include($$PWD/../tests.pri)

QT *= \
    appman_common-private \
    appman_crypto-private \
    appman_application-private \
    appman_package-private \
    appman_manager-private \
    appman_installer-private \

INCLUDEPATH += ../../src/tools/packager
SOURCES += ../../src/tools/packager/packager.cpp

SOURCES += tst_packager-tool.cpp
