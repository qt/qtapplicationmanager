
TARGET = tst_utilities

include(../tests.pri)

load(add-static-library)
addStaticLibrary(../../src/common-lib)

SOURCES += tst_utilities.cpp
