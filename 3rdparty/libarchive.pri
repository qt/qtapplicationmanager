
include(libz.pri)

!config_libarchive|no-system-libarchive {
    load(fix-library-path)

    LIBARCHIVE_PATH = $$PWD/libarchive
    LIBARCHIVE_BUILD_PATH = $$shadowed($$LIBARCHIVE_PATH)

    INCLUDEPATH += $$LIBARCHIVE_PATH/libarchive
    LIBS_PRIVATE += $$fixLibraryPath(-L$$LIBARCHIVE_BUILD_PATH) -lqtarchive$$qtPlatformTargetSuffix()

    osx:LIBS_PRIVATE += -framework CoreServices -liconv
    win32:LIBS_PRIVATE += -lcrypt32
    win32:DEFINES += LIBARCHIVE_STATIC

    CONFIG *= link_prl
} else {
    PKGCONFIG_PRIVATE += "'libarchive >= 3.1.2'"
    CONFIG *= link_pkgconfig
}
