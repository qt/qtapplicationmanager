TEMPLATE = app
CONFIG += am-systemui

OTHER_FILES += \
    am-config.yaml \
    system-ui/*.qml \
    apps/tld.multi-views.app/*.yaml \
    apps/tld.multi-views.app/*.qml \
    apps/tld.multi-views.app/*.png \

target.path = $$[QT_INSTALL_EXAMPLES]/applicationmanager/multi-views
INSTALLS += target

AM_COPY_DIRECTORIES += apps system-ui
AM_COPY_FILES += am-config.yaml

prefix_build:tpath = $$target.path
else:tpath = $$_PRO_FILE_PWD_

AM_DEFAULT_ARGS = -c $$tpath/am-config.yaml --start-session-dbus --verbose -r

example_sources.path = $$target.path
example_sources.files = $$AM_COPY_FILES $$AM_COPY_DIRECTORIES
INSTALLS += example_sources
