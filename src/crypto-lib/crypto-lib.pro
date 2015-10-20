
TEMPLATE = lib
TARGET = crypto-lib

include($$BASE_PRI)

CONFIG *= static create_prl

QT = core

DEFINES *= AM_BUILD_APPMAN

addStaticLibrary(../common-lib)

include($$SOURCE_DIR/3rdparty/libcrypto.pri)

# Apple has deprecated all OpenSSL calls in 10.7
osx:QMAKE_CXXFLAGS += -Wno-deprecated-declarations

SOURCES += \
    cipherfilter.cpp \
    cryptography.cpp \
    digestfilter.cpp \
    signature.cpp \

HEADERS += \
    cipherfilter.h \
    cryptography.h \
    digestfilter.h \
    signature.h \
