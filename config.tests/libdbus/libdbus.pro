TARGET = libdbus
CONFIG -= qt

SOURCES += main.cpp

PKGCONFIG += "'dbus-1 >= 1.8'"
CONFIG += link_pkgconfig
