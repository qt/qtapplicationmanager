
TARGET = tst_runtime

include(../tests.pri)

QT *= qml

load(add-static-library)
addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/manager-lib)

SOURCES += tst_runtime.cpp
