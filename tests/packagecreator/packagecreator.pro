
TARGET = tst_packagecreator

include(../tests.pri)

load(add-static-library)
addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/manager-lib)
addStaticLibrary(../../src/installer-lib)

SOURCES += tst_packagecreator.cpp
