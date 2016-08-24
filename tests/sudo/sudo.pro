
TARGET    = tst_sudo

COVERAGE_RUNTIME = sudo

include(../tests.pri)

load(add-static-library)
addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/installer-lib)

SOURCES += tst_sudo.cpp
