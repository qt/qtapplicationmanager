TEMPLATE = app
CONFIG += am-systemui

OTHER_FILES += \
    am-config.yaml \
    application-features.qmlproject \
    doc/src/*.qdoc \
    doc/images/*.png \
    system-ui/*.qml \
    system-ui/*.png \
    apps/compositor/*.yaml \
    apps/compositor/*.qml \
    apps/compositor/*.png \
    apps/crash/*.yaml \
    apps/crash/*.qml \
    apps/crash/*.png \
    apps/twins/*.yaml \
    apps/twins/*.qml \
    apps/twins/*.png \
    apps/widgets/*.yaml \
    apps/widgets/*.png

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/application-features
INSTALLS += target

AM_COPY_DIRECTORIES += apps system-ui
AM_COPY_FILES += am-config.yaml

prefix_build:tpath = $$target.path
else:tpath = $$OUT_PWD

AM_DEFAULT_ARGS = -c $$tpath/am-config.yaml

example_sources.path = $$target.path
example_sources.files = $$AM_COPY_FILES $$AM_COPY_DIRECTORIES doc
INSTALLS += example_sources
