# Generated from libbacktrace.pro.

#####################################################################
## BundledBacktrace Generic Library:
#####################################################################

qt_internal_add_3rdparty_library(BundledBacktrace
    QMAKE_LIB_NAME backtrace
    STATIC
    INSTALL
    SOURCES
        libbacktrace/atomic.c
        libbacktrace/backtrace.c
        libbacktrace/dwarf.c
        libbacktrace/fileline.c
        libbacktrace/mmap.c
        libbacktrace/mmapio.c
        libbacktrace/posix.c
        libbacktrace/print.c
        libbacktrace/simple.c
        libbacktrace/sort.c
        libbacktrace/state.c
    DEFINES
        _GNU_SOURCE
    INCLUDE_DIRECTORIES
        auxincl
        libbacktrace
    PUBLIC_INCLUDE_DIRECTORIES
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
qt_disable_warnings(BundledBacktrace)
qt_set_symbol_visibility_hidden(BundledBacktrace)

#####################################################################

#### Keys ignored in scope 2:.:.:libbacktrace.pro:win32-msvc_x_:
# QMAKE_CFLAGS = "/D_CRT_SECURE_NO_WARNINGS"

#### Keys ignored in scope 3:.:.:libbacktrace.pro:GCC:
# QMAKE_CFLAGS = "-Wno-unused" "-funwind-tables" "-Wno-switch" "-Wno-enum-compare"

#### Keys ignored in scope 4:.:.:libbacktrace.pro:CLANG:
# QMAKE_CFLAGS = "-Wall" "-W" "-Wno-unused"

qt_internal_extend_target(BundledBacktrace CONDITION LINUX
    SOURCES
        libbacktrace/elf.c
)

qt_internal_extend_target(BundledBacktrace CONDITION MACOS
    SOURCES
        libbacktrace/macho.c
)
