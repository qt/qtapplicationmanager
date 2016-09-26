TARGET = tst_installationreport

include($$PWD/../tests.pri)

QT *= \
    appman_common-private \
    appman_crypto-private \
    appman_application-private \

SOURCES += tst_installationreport.cpp
