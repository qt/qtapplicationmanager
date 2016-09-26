TARGET = tst_signature

include($$PWD/../tests.pri)

QT *= appman_common-private appman_crypto-private

SOURCES += tst_signature.cpp

RESOURCES += tst_signature.qrc
