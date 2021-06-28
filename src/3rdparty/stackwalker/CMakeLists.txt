
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
