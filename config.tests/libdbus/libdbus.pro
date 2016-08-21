TARGET = libdbus
CONFIG -= qt
CONFIG += console

SOURCES += main.cpp

PKGCONFIG += "'dbus-1 >= 1.6'"
CONFIG += link_pkgconfig
