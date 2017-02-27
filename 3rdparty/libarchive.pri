
include(libz.pri)

!config_libarchive|no-system-libarchive {
    lessThan(QT_MINOR_VERSION, 8) {
        # this is the "old way" used by Qt <= 5.7
        load(fix-library-path)

        LIBARCHIVE_PATH = $$PWD/libarchive
        LIBARCHIVE_BUILD_PATH = $$shadowed($$LIBARCHIVE_PATH)

        INCLUDEPATH += $$LIBARCHIVE_PATH/libarchive
        LIBS_PRIVATE += $$fixLibraryPath(-L$$LIBARCHIVE_BUILD_PATH) -lqtarchive$$qtPlatformTargetSuffix()

        win32:DEFINES += LIBARCHIVE_STATIC

        CONFIG *= link_prl
    } else {
        # the "new way", starting with Qt 5.8
        QMAKE_USE_PRIVATE += archive
    }
} else {
    PKGCONFIG_PRIVATE += "'libarchive >= 3.1.2'"
    CONFIG *= link_pkgconfig
}
