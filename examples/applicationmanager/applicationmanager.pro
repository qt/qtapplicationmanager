# temporarily disabled because the module.pri files generated from the cmake
# are wrong: e.g. appman_main-private depends on appman_common, while it should
# depend on appman_common-private
requires(false)

load(am-config)

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
    custom-appman \

linux:!android:SUBDIRS += \
    softwarecontainer-plugin \
