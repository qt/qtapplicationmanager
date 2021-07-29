
TEMPLATE = subdirs

SUBDIRS = \
    animated-windows \
    frame-timer \
    hello-world \
    launch-intents \
    minidesk \
    application-features \
    multi-views \
    process-status \
    startup-plugin \
    intents \
    custom-appman \

linux:!android:SUBDIRS += \
    softwarecontainer-plugin \
