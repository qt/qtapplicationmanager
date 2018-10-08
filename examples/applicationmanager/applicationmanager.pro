TEMPLATE = subdirs

SUBDIRS = \
    animated-windows \
    hello-world \
    minidesk \
    monitor \
    multi-views \
    startup-plugin \
    intents \

# remove the !headless and handle this in the example when we switch to the new configure system
!headless:SUBDIRS += \
    custom-appman \

linux:!android:SUBDIRS += \
    softwarecontainer-plugin \
