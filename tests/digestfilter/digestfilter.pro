TARGET = tst_digestfilter

include($$PWD/../tests.pri)

QT *= appman_common-private appman_crypto-private

SOURCES += tst_digestfilter.cpp

OTHER_FILES += message.bin
