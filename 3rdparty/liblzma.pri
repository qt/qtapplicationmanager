
bundled-liblzma {
    error("bundled-liblzma is not supported ATM")

#    LIBLZMA_PATH = $$PWD/xz-5.2.1
#    LIBLZMA_BUILD_PATH = $$shadowed($$LIBLZMA_PATH)
#
#    INCLUDEPATH += $$LIBLZMA_PATH/src/liblzma/api
#    LIBS += $$fixLibraryPath(-L$$LIBLZMA_BUILD_PATH) -llzma
#
#    win32:DEFINES += LZMA_API_STATIC
#
#    CONFIG *= link_prl
} else {
    osx:exists(/usr/local/bin/pkg-config):PKG_CONFIG=/usr/local/bin/pkg-config

    PKGCONFIG += liblzma
}


