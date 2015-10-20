
TARGET = tst_cipherfilter

include(../tests.pri)

addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/crypto-lib)

SOURCES += tst_cipherfilter.cpp

RESOURCES += tst_cipherfilter.qrc
