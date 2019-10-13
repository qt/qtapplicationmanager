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

GIT_VERSION = $$cat($$SOURCE_DIR/.tag, lines)
equals(GIT_VERSION, "\$Format:%H\$") {
  unix:exists($$SOURCE_DIR/.git):GIT_VERSION=$$system(cd "$$SOURCE_DIR" && git describe --tags --always --dirty 2>/dev/null)
  else:GIT_VERSION="unknown"
}

createBuildConfig(_DATE_, MODULE_VERSION, GIT_VERSION, SOURCE_DIR, BUILD_DIR, INSTALL_PREFIX, \
                  QT_ARCH, QT_VERSION, QT_CONFIG, CONFIG, DEFINES, INCLUDEPATH, LIBS)

# For android installing the binaries doesn't make sense, as it's a command-line utility which doesn't work on android anyway.
android:INSTALLS=
