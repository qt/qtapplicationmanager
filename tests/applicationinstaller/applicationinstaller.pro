TARGET = tst_applicationinstaller

COVERAGE_RUNTIME = sudo

include($$PWD/../tests.pri)

QT *= \
    appman_common-private \
    appman_application-private \
    appman_package-private \
    appman_manager-private \

SOURCES += tst_applicationinstaller.cpp
