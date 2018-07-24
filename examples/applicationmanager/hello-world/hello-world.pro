TEMPLATE = app
CONFIG += am-systemui

OTHER_FILES += \
    am-config.yaml \
    doc/images/*.png \
    doc/src/*.qdoc \
    system-ui/*.qml \
    apps/hello-world.blue/*.yaml \
    apps/hello-world.blue/*.qml \
    apps/hello-world.blue/*.png \
    apps/hello-world.green/*.yaml \
    apps/hello-world.green/*.qml \
    apps/hello-world.green/*.png \
    apps/hello-world.red/*.yaml \
    apps/hello-world.red/*.qml \
    apps/hello-world.red/*.png \

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/hello-world
INSTALLS += target

AM_COPY_DIRECTORIES += apps system-ui
AM_COPY_FILES += am-config.yaml

AM_DEFAULT_ARGS=-c am-config.yaml --start-session-dbus --verbose -r

example_sources.path = $$target.path
example_sources.files = $$AM_COPY_FILES $$AM_COPY_DIRECTORIES
INSTALLS += example_sources
