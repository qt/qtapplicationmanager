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

BACKTRACE_SUPPORTED = 1
BACKTRACE_SUPPORTS_THREADS = 1
BACKTRACE_USES_MALLOC = 0

is64bit = $$find(QMAKE_HOST.arch, "64$")
!isEmpty(is64bit):BACKTRACE_ELF_SIZE = 64
else:BACKTRACE_ELF_SIZE = 32

configin.input = $$PWD/libbacktrace/config.h.in
configin.output = $$OUT_PWD/libbacktrace/config.h
QMAKE_SUBSTITUTES += configin

supportedin.input = $$PWD/libbacktrace/backtrace-supported.h.in
supportedin.output = $$OUT_PWD/libbacktrace/backtrace-supported.h
QMAKE_SUBSTITUTES += supportedin
#QMAKE_SUBSTITUTES += libbacktrace/config.h.in  libbacktrace/backtrace-supported.h.in

OTHER_FILES += \
    $$configin.input \
    $$supportedin.input \

INCLUDEPATH += $$PWD/auxincl $$PWD/libbacktrace $$OUT_PWD/libbacktrace

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
