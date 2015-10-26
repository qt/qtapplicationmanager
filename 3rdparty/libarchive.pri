
include(libz.pri)

bundled-libarchive {
    LIBARCHIVE_PATH = $$PWD/libarchive
    LIBARCHIVE_BUILD_PATH = $$shadowed($$LIBARCHIVE_PATH)

    INCLUDEPATH += $$LIBARCHIVE_PATH/libarchive
    osx:LIBS += $$join(LIBARCHIVE_BUILD_PATH,,,/libarchive.a) -framework CoreServices -liconv
    else:LIBS += $$fixLibraryPath(-L$$LIBARCHIVE_BUILD_PATH) -larchive

    win32:DEFINES += LIBARCHIVE_STATIC

    CONFIG *= link_prl
} else {
    PKGCONFIG += "'libarchive >= 3.1.2'"
}
