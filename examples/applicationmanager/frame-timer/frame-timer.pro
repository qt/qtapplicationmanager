TEMPLATE = app
CONFIG += am-systemui

OTHER_FILES += \
    am-config.yaml \
    system-ui/*.qml \
    doc/images/*.png \
    doc/src/*.qdoc \
    apps/frame-timer.fish/*.yaml \
    apps/frame-timer.fish/*.qml \
    apps/frame-timer.fish/*.svg \
    apps/frame-timer.rabbit/*.yaml \
    apps/frame-timer.rabbit/*.qml \
    apps/frame-timer.rabbit/*.svg \

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/frame-timer
INSTALLS += target

AM_COPY_DIRECTORIES += apps system-ui
AM_COPY_FILES += am-config.yaml

prefix_build:tpath = $$target.path
else:tpath = $$_PRO_FILE_PWD_

AM_DEFAULT_ARGS = -c $$tpath/am-config.yaml --verbose

example_sources.path = $$target.path
example_sources.files = $$AM_COPY_FILES $$AM_COPY_DIRECTORIES
INSTALLS += example_sources
