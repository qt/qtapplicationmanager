TEMPLATE = app
CONFIG += install_ok
QT += widgets appman_launcher-private

SOURCES = main.cpp

DESTDIR = $$OUT_PWD/../../apps/widgets

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/application-features/apps/widgets
INSTALLS += target
