
TEMPLATE = app
TARGET   = appman-packager
DESTDIR  = $$BUILD_DIR/bin

include($$BASE_PRI)

CONFIG *= console
QT = core network

DEFINES *= AM_BUILD_APPMAN

addStaticLibrary(../../common-lib)
addStaticLibrary(../../crypto-lib)
addStaticLibrary(../../manager-lib)
addStaticLibrary(../../installer-lib)

target.path = $$INSTALL_PREFIX/bin/
INSTALLS += target

SOURCES += \
    main.cpp \
    packager.cpp

HEADERS += \
    packager.h

OTHER_FILES += \
    README
