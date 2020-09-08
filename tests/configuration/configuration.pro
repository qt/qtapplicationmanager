TARGET = tst_configuration

include($$PWD/../tests.pri)

QT *= appman_common-private appman_main-private

SOURCES += tst_configuration.cpp

RESOURCES += \
    data/empty.yaml \
    data/config1.yaml \
    data/config2.yaml \
