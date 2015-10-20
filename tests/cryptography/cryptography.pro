
TARGET = tst_cryptography

include(../tests.pri)

addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/crypto-lib)

SOURCES += tst_cryptography.cpp
