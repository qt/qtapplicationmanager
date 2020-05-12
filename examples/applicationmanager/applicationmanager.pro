load(am-config)

requires(!headless)
requires(!disable-installer)

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

# remove the !headless and handle this in the example when we switch to the new configure system
SUBDIRS += \
    custom-appman \

linux:!android:SUBDIRS += \
    softwarecontainer-plugin \
