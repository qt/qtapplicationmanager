TEMPLATE = subdirs

load(am-config)

multi-process:qtHaveModule(dbus):SUBDIRS = qml
