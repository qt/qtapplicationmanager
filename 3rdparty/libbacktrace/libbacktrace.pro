requires(linux)

TEMPLATE = lib
TARGET = backtrace

load(am-config)

CONFIG -= qt
CONFIG += staticlib create_prl

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
