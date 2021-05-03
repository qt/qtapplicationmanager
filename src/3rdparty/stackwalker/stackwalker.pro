requires(windows:msvc)

TEMPLATE = lib
TARGET = stackwalker

load(am-config)

CONFIG += \
    static \
    hide_symbols \
    warn_off \
    installed

MODULE_INCLUDEPATH += $$PWD

load(qt_helper_lib)

QMAKE_CFLAGS += /D_CRT_SECURE_NO_WARNINGS

INCLUDEPATH += $$PWD

HEADERS += \
    stackwalker.h

SOURCES += \
    stackwalker.cpp
