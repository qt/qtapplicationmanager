TEMPLATE = lib
TARGET = app1plugin
CONFIG += plugin
RESOURCES = app1plugin.qrc

RESOURCE_SOURCE = app1file.qrc
load(generate-resource)

OTHER_FILES += \
    info.yaml \
    icon.png
