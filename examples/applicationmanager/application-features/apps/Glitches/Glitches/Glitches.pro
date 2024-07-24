TEMPLATE = lib
CONFIG += plugin qmltypes exceptions install_ok
QT += qml quick

QML_IMPORT_NAME = Glitches
QML_IMPORT_MAJOR_VERSION = 1

TARGET = $$qtLibraryTarget(glitchesmodule)

HEADERS += glitches.h glitchesplugin.h
SOURCES += glitches.cpp

OTHER_FILES += qmldir

qmldir_copy.files = $$PWD/qmldir
qmldir_copy.path = $$OUT_PWD
COPIES += qmldir_copy

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/application-features/apps/Glitches/Glitches
qmldir.files = $$PWD/qmldir
qmldir.path = $${target.path}
INSTALLS += target qmldir
