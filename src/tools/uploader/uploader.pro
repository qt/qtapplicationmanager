TEMPLATE = app
TARGET   = package-uploader

load(am-config)

QT = core network
QT *= appman_common-private

CONFIG *= console

SOURCES += \
    uploader.cpp \

HEADERS += \

load(qt_tool)

load(install-prefix)
