
include(../tests.pri)

TARGET = tst_packageextractor

load(add-static-library)
addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/manager-lib)
addStaticLibrary(../../src/installer-lib)

SOURCES += tst_packageextractor.cpp
