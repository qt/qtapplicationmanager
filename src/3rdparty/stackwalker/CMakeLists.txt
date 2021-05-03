# Generated from stackwalker.pro.

#####################################################################
## BundledStackwalker Generic Library:
#####################################################################

qt_internal_add_3rdparty_library(BundledStackwalker
    QMAKE_LIB_NAME stackwalker
    STATIC
    INSTALL
    SOURCES
        stackwalker.cpp stackwalker.h
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC_INCLUDE_DIRECTORIES
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
qt_disable_warnings(BundledStackwalker)
qt_set_symbol_visibility_hidden(BundledStackwalker)

#### Keys ignored in scope 1:.:.:stackwalker.pro:<TRUE>:
# QMAKE_CFLAGS = "/D_CRT_SECURE_NO_WARNINGS"
# TEMPLATE = "lib"
# _REQUIREMENTS = "windows && msvc"
