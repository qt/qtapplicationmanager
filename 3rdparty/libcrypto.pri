
include(libz.pri)

bundled-libcrypto {
    error("bundled-libcrypto is not supported ATM")

#    LIBCRYPTO_PATH = $$PWD/openssl-1.0.2d
#    LIBCRYPTO_BUILD_PATH = $$shadowed($$LIBCRYPTO_PATH)
#
#    unix {
#        INCLUDEPATH += $$LIBCRYPTO_PATH/include
#        LIBS += -L$$LIBCRYPTO_BUILD_PATH -lcrypto -ldl
#    }
#    win32 {
#        INCLUDEPATH += $$LIBCRYPTO_PATH/libcrypto-include-win32
#        LIBS += $$fixLibraryPath(-L$$LIBCRYPTO_BUILD_PATH) -lcrypto -luser32 -ladvapi32 -lgdi32
#    }
#
#    CONFIG *= link_prl
} else {
    osx:exists(/usr/local/bin/pkg-config):PKG_CONFIG=/usr/local/bin/pkg-config

    PKGCONFIG += "'libcrypto >= 1.0.0'"
}
