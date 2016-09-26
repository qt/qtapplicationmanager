TARGET = tst_cryptography

include($$PWD/../tests.pri)

QT *= appman_common-private appman_crypto-private

SOURCES += tst_cryptography.cpp
