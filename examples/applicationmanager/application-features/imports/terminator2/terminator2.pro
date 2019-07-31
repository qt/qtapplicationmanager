TEMPLATE = lib
CONFIG += plugin exceptions install_ok
QT += qml quick

TARGET = $$qtLibraryTarget(terminator2plugin)

HEADERS += qmlterminator2.h
SOURCES += qmlterminator2.cpp

OTHER_FILES += qmldir

DESTDIR = $$OUT_PWD/../../apps/crash/Terminator
target.path=$$DESTDIR
qmldir.files=$$PWD/qmldir
qmldir.path=$$DESTDIR

INSTALLS += target
COPIES += qmldir
