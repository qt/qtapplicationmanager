
TARGET    = tst_sudo

COVERAGE_RUNTIME = sudo

include(../tests.pri)

addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/installer-lib)

SOURCES += tst_sudo.cpp

