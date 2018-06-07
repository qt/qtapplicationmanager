TEMPLATE = app
CONFIG += am-systemui

OTHER_FILES += \
    am-config.yaml \
    minidesk.qmlproject \
    doc/src/*.qdoc \
    doc/images/*.png \
    system-ui/*.qml \
    apps/tld.minidesk.app1/*.yaml \
    apps/tld.minidesk.app1/*.qml \
    apps/tld.minidesk.app1/*.png \
    apps/tld.minidesk.app2/*.yaml \
    apps/tld.minidesk.app2/*.qml \
    apps/tld.minidesk.app2/*.png \

target.path = $$[QT_INSTALL_EXAMPLES]/minidesk
INSTALLS += target

AM_COPY_DIRECTORIES += apps system-ui
AM_COPY_FILES += am-config.yaml

AM_DEFAULT_ARGS=-c am-config.yaml --start-session-dbus --verbose
