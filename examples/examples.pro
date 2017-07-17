TEMPLATE = subdirs

SUBDIRS = \
    minidesk \
    monitor \
    startup-plugin \
    custom-appman \

linux:!android:SUBDIRS += \
    softwarecontainer-plugin \
