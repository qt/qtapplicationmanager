TEMPLATE = lib
CONFIG += plugin exceptions install_ok
QT += qml quick

TARGET = $$qtLibraryTarget(terminator2plugin)

HEADERS += qmlterminator2.h
SOURCES += qmlterminator2.cpp

OTHER_FILES += qmldir

DESTDIR = $$OUT_PWD/../../apps/crash/Terminator

qmldir_copy.files = $$PWD/qmldir
qmldir_copy.path = $$DESTDIR
COPIES += qmldir_copy

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/application-features/apps/crash/Terminator
qmldir.files = $$PWD/qmldir
qmldir.path = $${target.path}
INSTALLS += target qmldir
