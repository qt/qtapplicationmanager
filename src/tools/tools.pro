
TEMPLATE = subdirs

include($$BASE_PRI)

!android:SUBDIRS += \
    packager \
    deployer \

qtHaveModule(dbus):SUBDIRS += \
    controller \
