
TEMPLATE = subdirs

!android:!windows:!osx:qtHaveModule(dbus):SUBDIRS = qml
