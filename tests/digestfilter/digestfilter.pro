
TARGET = tst_digestfilter

include(../tests.pri)

load(add-static-library)
addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/crypto-lib)

SOURCES += tst_digestfilter.cpp

OTHER_FILES += message.bin
