
TEMPLATE = subdirs

include($$BASE_PRI)

!android:!windows:!osx:qtHaveModule(dbus):SUBDIRS = qml
