TARGET = libdbus
CONFIG -= qt
CONFIG += console

SOURCES += main.cpp

PKGCONFIG += "'dbus-1 >= 1.8'"
CONFIG += link_pkgconfig
