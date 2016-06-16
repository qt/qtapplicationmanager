TARGET = libcrypto
CONFIG -= qt
CONFIG += console

SOURCES += main.cpp

linux:packagesExist("'libcrypto >= 1.0.1'") {
    PKGCONFIG += libcrypto
    CONFIG *= link_pkgconfig
}
