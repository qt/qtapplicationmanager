
include(libz.pri)
#include(liblzma.pri)

bundled-libarchive {
    error("bundled-libarchive is not supported ATM")

#    LIBARCHIVE_PATH = $$PWD/libarchive-3.1.2
#    LIBARCHIVE_BUILD_PATH = $$shadowed($$LIBARCHIVE_PATH)
#
#    INCLUDEPATH += $$LIBARCHIVE_PATH/libarchive
#    osx:LIBS += $$join(LIBARCHIVE_BUILD_PATH,,,/libarchive.a) -framework CoreServices -liconv
#    else:LIBS += $$fixLibraryPath(-L$$LIBARCHIVE_BUILD_PATH) -larchive
#
#    win32:DEFINES += LIBARCHIVE_STATIC
#
#    CONFIG *= link_prl
} else {
    osx:exists(/usr/local/bin/pkg-config):PKG_CONFIG=/usr/local/bin/pkg-config

    PKGCONFIG += "'libarchive >= 3.0.0'"
} 
