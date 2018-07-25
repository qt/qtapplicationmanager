TEMPLATE = app
CONFIG += am-systemui

OTHER_FILES += \
    am-config.yaml \
    system-ui/*.qml \
    apps/animated-windows.fish/*.yaml \
    apps/animated-windows.fish/*.qml \
    apps/animated-windows.fish/*.svg \
    apps/animated-windows.rabbit/*.yaml \
    apps/animated-windows.rabbit/*.qml \
    apps/animated-windows.rabbit/*.svg \
    doc/images/*.png \
    doc/src/*.qdoc \

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/animated-windows
INSTALLS += target

AM_COPY_DIRECTORIES += apps system-ui
AM_COPY_FILES += am-config.yaml

AM_DEFAULT_ARGS=-c am-config.yaml --start-session-dbus --verbose -r

example_sources.path = $$target.path
example_sources.files = $$AM_COPY_FILES $$AM_COPY_DIRECTORIES
INSTALLS += example_sources
