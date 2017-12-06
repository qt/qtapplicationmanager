TEMPLATE = app
TARGET = appman

load(am-config)

QT = appman_main-private

CONFIG *= console

SOURCES += \
    $$PWD/appman.cpp

load(qt_tool)

load(install-prefix)

load(build-config)

unix:exists($$SOURCE_DIR/.git):GIT_VERSION=$$system(cd "$$SOURCE_DIR" && git describe --tags --always --dirty 2>/dev/null)
isEmpty(GIT_VERSION):GIT_VERSION="unknown"

createBuildConfig(_DATE_, VERSION, GIT_VERSION, SOURCE_DIR, BUILD_DIR, INSTALL_PREFIX, \
                  QT_ARCH, QT_VERSION, QT, CONFIG, DEFINES, INCLUDEPATH, LIBS)
