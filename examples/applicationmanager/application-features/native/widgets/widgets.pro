TEMPLATE = app
CONFIG += install_ok
QT += widgets appman_launcher-private appman_common-private

SOURCES = main.cpp

DESTDIR = $$OUT_PWD/../../apps/widgets
target.path=$$DESTDIR
INSTALLS += target
