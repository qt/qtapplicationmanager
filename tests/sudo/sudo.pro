TARGET    = tst_sudo

CONFIG += insignificant_test # the CI cannot run this test via sudo

COVERAGE_RUNTIME = sudo

include($$PWD/../tests.pri)

QT *= \
    appman_common-private \
    appman_installer-private \

SOURCES += tst_sudo.cpp
