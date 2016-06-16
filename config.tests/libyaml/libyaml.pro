TARGET = libyaml
CONFIG -= qt
CONFIG += console

SOURCES += main.cpp

PKGCONFIG += "'yaml-0.1 >= 0.1.6'"
CONFIG += link_pkgconfig
