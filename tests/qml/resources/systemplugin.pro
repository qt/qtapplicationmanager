RESOURCE_SOURCE = systemuifile.qrc
load(generate-resource)

TEMPLATE = lib
TARGET = systemuiplugin
CONFIG += plugin
CONFIG -= debug_and_release
RESOURCES = systemuiplugin.qrc
