TARGET = libarchive
CONFIG -= qt

SOURCES += main.cpp

PKGCONFIG += "'libarchive >= 3.1.2'"
CONFIG += link_pkgconfig
