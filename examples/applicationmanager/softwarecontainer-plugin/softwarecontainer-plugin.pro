requires(linux:!android)

TEMPLATE = lib
CONFIG += plugin c++11
TARGET = softwarecontainer-plugin

QT = core dbus appman_common-private appman_plugininterfaces-private

HEADERS += \
    softwarecontainer.h

SOURCES += \
    softwarecontainer.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/softwarecontainer-plugin
INSTALLS += target

DISTFILES += \
    service-manifest.d/io.qt.ApplicationManager.Application.json

OTHER_FILES += README.md
