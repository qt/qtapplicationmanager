TARGET = touchemulation
CONFIG -= qt
CONFIG += console

SOURCES += main.cpp

PKGCONFIG += xcb x11 xi
CONFIG += link_pkgconfig
