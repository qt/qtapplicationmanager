
TARGET = tst_applicationinstaller

COVERAGE_RUNTIME = sudo

include(../tests.pri)

addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/manager-lib)
addStaticLibrary(../../src/installer-lib)

HEADERS +=

SOURCES += tst_applicationinstaller.cpp
