
TARGET = tst_packager-tool

include(../tests.pri)

addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/crypto-lib)
addStaticLibrary(../../src/manager-lib)
addStaticLibrary(../../src/installer-lib)

INCLUDEPATH += ../../src/tools/packager
SOURCES += ../../src/tools/packager/packager.cpp

SOURCES += tst_packager-tool.cpp
