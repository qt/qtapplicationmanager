TARGET = tst_applicationinstaller

CONFIG *= insignificant_test # the CI cannot run this test via sudo

COVERAGE_RUNTIME = sudo

include($$PWD/../tests.pri)

QT *= \
    appman_common-private \
    appman_application-private \
    appman_package-private \
    appman_manager-private \
    appman_installer-private

SOURCES += tst_applicationinstaller.cpp
