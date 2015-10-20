
TARGET = tst_application

include(../tests.pri)

addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/manager-lib)

SOURCES += tst_application.cpp
