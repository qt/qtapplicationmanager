
TARGET = tst_packagecreator

include(../tests.pri)

addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/manager-lib)
addStaticLibrary(../../src/installer-lib)

SOURCES += tst_packagecreator.cpp
