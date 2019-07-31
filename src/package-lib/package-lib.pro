TEMPLATE = lib
TARGET = QtAppManPackage
MODULE = appman_package

load(am-config)

QT = core network
QT_FOR_PRIVATE *= \
    appman_common-private \
    appman_crypto-private \
    appman_application-private \

CONFIG *= static internal_module
CONFIG -= create_cmake

include($$SOURCE_DIR/3rdparty/libarchive.pri)
include($$SOURCE_DIR/3rdparty/libz.pri)

HEADERS += \
    package_p.h \
    packageextractor_p.h \
    packageextractor.h \
    packagecreator_p.h \
    packagecreator.h \
    package.h

SOURCES += \
    package_p.cpp \
    packagecreator.cpp \
    packageextractor.cpp \
    package.cpp

load(qt_module)
