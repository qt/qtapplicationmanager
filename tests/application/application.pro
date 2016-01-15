
TARGET = tst_application

include(../tests.pri)

load(add-static-library)
addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/manager-lib)

SOURCES += tst_application.cpp
