TEMPLATE = lib
CONFIG += plugin exceptions
QT += qml quick

TARGET = $$qtLibraryTarget(terminator2plugin)

HEADERS += qmlterminator2.h
SOURCES += qmlterminator2.cpp

DESTDIR = $$_PRO_FILE_PWD_/Terminator
target.path=$$DESTDIR
qmldir.files=$$PWD/qmldir
qmldir.path=$$DESTDIR
INSTALLS += target qmldir

OTHER_FILES += qmldir

QMAKE_POST_LINK += $$QMAKE_COPY $$replace($$list($$quote($$PWD/qmldir) $$DESTDIR), /, $$QMAKE_DIR_SEP)
