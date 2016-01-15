
TARGET = tst_installationreport

include(../tests.pri)

load(add-static-library)
addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/crypto-lib)
addStaticLibrary(../../src/manager-lib)

SOURCES += tst_installationreport.cpp
