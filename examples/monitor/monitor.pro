TEMPLATE = app
CONFIG += am-systemui

OTHER_FILES += \
    am-config.yaml \
    monitor.qmlproject \
    doc/src/*.qdoc \
    doc/images/*.png \
    system-ui/*.qml \
    apps/tld.monitor.app/*.yaml \
    apps/tld.monitor.app/*.qml

target.path = $$[QT_INSTALL_EXAMPLES]/monitor
INSTALLS += target

AM_COPY_DIRECTORIES += apps system-ui
AM_COPY_FILES += am-config.yaml

AM_DEFAULT_ARGS=-c am-config.yaml --start-session-dbus --verbose
