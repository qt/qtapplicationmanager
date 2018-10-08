TEMPLATE = app
CONFIG += am-systemui

OTHER_FILES += \
    *.qml \
    apps/hello-world.blue/*.yaml \
    apps/hello-world.blue/*.qml \
    apps/hello-world.blue/*.png \
    apps/hello-world.green/*.yaml \
    apps/hello-world.green/*.qml \
    apps/hello-world.green/*.png \
    apps/hello-world.red/*.yaml \
    apps/hello-world.red/*.qml \
    apps/hello-world.red/*.png \

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/intents
INSTALLS += target

AM_COPY_DIRECTORIES += apps
AM_COPY_FILES += system-ui.qml

AM_DEFAULT_ARGS=--builtin-apps-manifest-dir $$target.path/apps $$target.path/system-ui.qml

example_sources.path = $$target.path
example_sources.files = $$AM_COPY_FILES $$AM_COPY_DIRECTORIES
INSTALLS += example_sources
