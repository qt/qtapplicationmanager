TEMPLATE = app
CONFIG += am-systemui

OTHER_FILES += \
    *.qml \
    doc/images/*.png \
    doc/src/*.qdoc \
    shared/*.qml \
    apps/intents.blue/*.yaml \
    apps/intents.blue/*.qml \
    apps/intents.blue/*.png \
    apps/intents.green/*.yaml \
    apps/intents.green/*.qml \
    apps/intents.green/*.png \
    apps/intents.red/*.yaml \
    apps/intents.red/*.qml \
    apps/intents.red/*.png \

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/intents
INSTALLS += target

AM_COPY_DIRECTORIES += apps shared
AM_COPY_FILES += system-ui.qml

prefix_build:tpath = $$target.path
else:tpath = $$_PRO_FILE_PWD_

AM_DEFAULT_ARGS = --builtin-apps-manifest-dir $$tpath/apps -o \"ui: { style: Material }\" $$tpath/system-ui.qml

example_sources.path = $$target.path
example_sources.files = $$AM_COPY_FILES $$AM_COPY_DIRECTORIES
INSTALLS += example_sources
