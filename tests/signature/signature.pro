
TARGET = tst_signature

include(../tests.pri)

addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/crypto-lib)

SOURCES += tst_signature.cpp

RESOURCES += tst_signature.qrc
