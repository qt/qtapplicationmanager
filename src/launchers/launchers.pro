
TEMPLATE = subdirs

load(am-config)

!android:!windows:!osx:qtHaveModule(dbus):SUBDIRS = qml
