TEMPLATE = app
CONFIG += am-systemui

OTHER_FILES += \
    am-config.yaml \
    system-ui/*.qml \
    doc/images/*.png \
    doc/src/*.qdoc \
    apps/process-status.cpu-hog/*.yaml \
    apps/process-status.cpu-hog/*.qml \
    apps/process-status.cpu-hog/*.png \
    apps/process-status.mem-hog/*.yaml \
    apps/process-status.mem-hog/*.qml \
    apps/process-status.mem-hog/*.png \
    apps/process-status.slim/*.yaml \
    apps/process-status.slim/*.qml \
    apps/process-status.slim/*.png \

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/process-status
INSTALLS += target

AM_COPY_DIRECTORIES += apps system-ui
AM_COPY_FILES += am-config.yaml

prefix_build:tpath = $$target.path
else:tpath = $$_PRO_FILE_PWD_

AM_DEFAULT_ARGS = -c $$tpath/am-config.yaml --verbose

example_sources.path = $$target.path
example_sources.files = $$AM_COPY_FILES $$AM_COPY_DIRECTORIES
INSTALLS += example_sources
