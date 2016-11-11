
TEMPLATE = subdirs

SUBDIRS += \
    testrunner

!android:SUBDIRS += \
    packager \
    deployer \

qtHaveModule(dbus):SUBDIRS += \
    controller \
