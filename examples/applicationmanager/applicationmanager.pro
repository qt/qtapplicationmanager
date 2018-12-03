TEMPLATE = subdirs

SUBDIRS = \
    animated-windows \
    frame-timer \
    hello-world \
    minidesk \
    multi-views \
    process-status \
    startup-plugin \
    intents \

# remove the !headless and handle this in the example when we switch to the new configure system
!headless:SUBDIRS += \
    custom-appman \

linux:!android:SUBDIRS += \
    softwarecontainer-plugin \
