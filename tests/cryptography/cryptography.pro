
TARGET = tst_cryptography

include(../tests.pri)

load(add-static-library)
addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/crypto-lib)

SOURCES += tst_cryptography.cpp
