TARGET    = tst_sudo

COVERAGE_RUNTIME = sudo

include($$PWD/../tests.pri)

QT *= \
    appman_common-private \
    appman_manager-private \

SOURCES += tst_sudo.cpp
