TARGET = QtAppManCrypto
MODULE = appman_crypto

load(am-config)

QT = core
QT_FOR_PRIVATE *= appman_common-private

CONFIG *= static internal_module

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

    QT *= network
}

load(qt_module)
