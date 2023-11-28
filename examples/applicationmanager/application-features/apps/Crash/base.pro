TEMPLATE = lib
CONFIG += plugin qmltypes exceptions install_ok
QT += qml quick

QML_IMPORT_NAME = Crash
QML_IMPORT_MAJOR_VERSION = 1

TARGET = $$qtLibraryTarget(crashmodule)

HEADERS += terminator1.h
SOURCES += terminator1.cpp

RESOURCES = crash.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/application-features/apps/Crash
INSTALLS += target
