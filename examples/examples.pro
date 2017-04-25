TEMPLATE = subdirs

SUBDIRS = \
    minidesk \
    monitor \
    startup-plugin \

linux:!android:SUBDIRS += \
    softwarecontainer-plugin \
