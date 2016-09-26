TARGET = tst_runtime

include($$PWD/../tests.pri)

QT *= qml
QT *= \
    appman_common-private \
    appman_application-private \
    appman_manager-private \

SOURCES += tst_runtime.cpp
