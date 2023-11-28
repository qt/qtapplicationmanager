TEMPLATE = app
CONFIG += am-systemui

OTHER_FILES += \
    am-config.yaml \
    application-features.qmlproject \
    doc/src/*.qdoc \
    doc/images/*.png \
    SystemUi/*.qml \
    SystemUi/*.png \
    apps/Compositor/*.yaml \
    apps/Compositor/*.qml \
    apps/Compositor/*.png \
    apps/Crash/*.yaml \
    apps/Crash/*.qml \
    apps/Crash/*.png \
    apps/Twins/*.yaml \
    apps/Twins/*.qml \
    apps/Twins/*.png \
    apps/Widgets/*.yaml \
    apps/Widgets/*.png

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/application-features
INSTALLS += target

AM_COPY_DIRECTORIES += apps SystemUi
AM_COPY_FILES += am-config.yaml client.qml

prefix_build:tpath = $$target.path
else:tpath = $$OUT_PWD

AM_DEFAULT_ARGS = -c $$tpath/am-config.yaml

example_sources.path = $$target.path
example_sources.files = $$AM_COPY_FILES $$AM_COPY_DIRECTORIES doc
INSTALLS += example_sources
