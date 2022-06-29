TEMPLATE = app
CONFIG += install_ok
QT += widgets appman_launcher-private

SOURCES = main.cpp

# This define flags us as a "launcher" and allows us to link against the AppMan's private libraries
DEFINES *= AM_COMPILING_LAUNCHER

DESTDIR = $$OUT_PWD/../../apps/widgets

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/application-features/apps/widgets
INSTALLS += target
