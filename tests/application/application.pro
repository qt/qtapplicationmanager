TARGET = tst_application

include($$PWD/../tests.pri)

QT *= \
    appman_common-private \
    appman_application-private \
    appman_manager-private \
    appman_installer-private \

SOURCES += tst_application.cpp
