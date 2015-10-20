
TARGET = tst_runtime

include(../tests.pri)

QT *= qml

addStaticLibrary(../../src/common-lib)
addStaticLibrary(../../src/manager-lib)

SOURCES += tst_runtime.cpp
