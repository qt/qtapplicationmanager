TEMPLATE = lib
CONFIG += plugin qmltypes install_ok
QT += qml quick

QML_IMPORT_NAME = Sequel
QML_IMPORT_MAJOR_VERSION = 1

TARGET = $$qtLibraryTarget(sequelmoduleplugin)

HEADERS += terminator2.h sequelplugin.h
SOURCES += terminator2.cpp

RESOURCES = sequel.qrc

OTHER_FILES += qmldir

qmldir_copy.files = $$PWD/qmldir
qmldir_copy.path = $$OUT_PWD
COPIES += qmldir_copy

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/application-features/apps/Crash/Sequel
qmldir.files = $$PWD/qmldir
qmldir.path = $${target.path}
INSTALLS += target qmldir
