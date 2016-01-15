
TEMPLATE = subdirs

load(am-config)

!android:SUBDIRS += \
    packager \
    deployer \

qtHaveModule(dbus):SUBDIRS += \
    controller \
