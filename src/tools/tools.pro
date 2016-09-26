
TEMPLATE = subdirs

!android:SUBDIRS += \
    packager \
    deployer \

qtHaveModule(dbus):SUBDIRS += \
    controller \
