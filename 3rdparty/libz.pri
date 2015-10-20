
bundled-libz {
    error("bundled-libz is not supported ATM")

#    LIBZ_PATH = $$PWD/zlib-1.2.8
#    LIBZ_BUILD_PATH = $$shadowed($$LIBZ_PATH)
#
#    INCLUDEPATH += $$LIBZ_PATH
#    LIBS += $$fixLibraryPath(-L$$LIBZ_BUILD_PATH) -lz
#
#    CONFIG *= link_prl
} else {
    osx:exists(/usr/local/bin/pkg-config):PKG_CONFIG=/usr/local/bin/pkg-config

    PKGCONFIG += zlib
}


