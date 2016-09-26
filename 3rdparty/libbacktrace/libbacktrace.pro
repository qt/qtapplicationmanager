requires(linux)

TARGET = qtbacktrace

load(am-config)

CONFIG += \
    static \
    hide_symbols \
    exceptions_off rtti_off warn_off \
    installed

load(qt_helper_lib)

win32-msvc* {
    QMAKE_CFLAGS += /D_CRT_SECURE_NO_WARNINGS
}
*-g++* {
    QMAKE_CFLAGS += -Wno-unused -funwind-tables -Wno-switch -Wno-enum-compare
}
*-clang* {
    CONFIG += warn_off
    QMAKE_CFLAGS += -Wall -W -Wno-unused
}

DEFINES *= _GNU_SOURCE

INCLUDEPATH += $$PWD/auxincl $$PWD/libbacktrace

SOURCES += \
    libbacktrace/backtrace.c \
    libbacktrace/simple.c \
    libbacktrace/elf.c \
    libbacktrace/dwarf.c \
    libbacktrace/mmapio.c \
    libbacktrace/mmap.c \
    libbacktrace/atomic.c \
    libbacktrace/fileline.c \
    libbacktrace/posix.c \
    libbacktrace/print.c \
    libbacktrace/sort.c \
    libbacktrace/state.c \
