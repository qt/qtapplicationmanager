
TEMPLATE = lib
TARGET = crypto-lib

include($$BASE_PRI)

CONFIG *= static create_prl

QT = core

DEFINES *= AM_BUILD_APPMAN

addStaticLibrary(../common-lib)

SOURCES += \
    cryptography.cpp \
    digestfilter.cpp \
    signature.cpp \

HEADERS += \
    cryptography.h \
    digestfilter.h \
    digestfilter_p.h \
    signature.h \
    signature_p.h \


win32:LIBS += -ladvapi32

win32:!force-libcrypto {
    SOURCES += \
        digestfilter_win.cpp \
        signature_win.cpp \

    LIBS += -lcrypt32
} else:osx:!force-libcrypto {
    SOURCES += \
        digestfilter_osx.cpp \
        signature_osx.cpp \

    LIBS += -framework Security
    QT *= core-private
} else {
    include($$SOURCE_DIR/3rdparty/libcrypto.pri)

    SOURCES += \
        libcryptofunction.cpp \
        digestfilter_openssl.cpp \
        signature_openssl.cpp \

    HEADERS += \
        libcryptofunction.h \

}
