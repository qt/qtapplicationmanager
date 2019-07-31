TEMPLATE = lib
TARGET = QtAppManCrypto
MODULE = appman_crypto

load(am-config)

QT = core
QT_FOR_PRIVATE *= appman_common-private

CONFIG *= static internal_module
CONFIG -= create_cmake

SOURCES += \
    cryptography.cpp \
    signature.cpp \

HEADERS += \
    cryptography.h \
    signature.h \
    signature_p.h \


win32:LIBS += -ladvapi32

win32:!force-libcrypto {
    SOURCES += signature_win.cpp

    LIBS += -lcrypt32
} else:macos:!force-libcrypto {
    SOURCES += signature_macos.cpp

    LIBS += -framework CoreFoundation -framework Security
    QT *= core-private
} else {
    SOURCES += \
        libcryptofunction.cpp \
        signature_openssl.cpp \

    HEADERS += \
        libcryptofunction.h \

    QT *= network
}

load(qt_module)
