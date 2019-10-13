TEMPLATE = app
CONFIG += am-systemui

OTHER_FILES += \
    *.qml \
    doc/images/*.png \
    doc/src/*.qdoc \
    apps/launch-intents.blue/*.yaml \
    apps/launch-intents.blue/*.qml \
    apps/launch-intents.blue/*.png \
    apps/launch-intents.green/*.yaml \
    apps/launch-intents.green/*.qml \
    apps/launch-intents.green/*.png \
    apps/launch-intents.red/*.yaml \
    apps/launch-intents.red/*.qml \
    apps/launch-intents.red/*.png \

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/launch-intents
INSTALLS += target

AM_COPY_DIRECTORIES += apps
AM_COPY_FILES += system-ui.qml

prefix_build:tpath = $$target.path
else:tpath = $$_PRO_FILE_PWD_

AM_DEFAULT_ARGS=--builtin-apps-manifest-dir $$tpath/apps $$tpath/system-ui.qml

example_sources.path = $$target.path
example_sources.files = $$AM_COPY_FILES $$AM_COPY_DIRECTORIES
INSTALLS += example_sources
