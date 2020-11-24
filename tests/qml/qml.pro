load(am-config)

TEMPLATE = subdirs
SUBDIRS = \
    simple \
    windowmanager \
    windowmapping \
    windowitem \
    windowitem2 \
    installer \
    quicklaunch \
    intents \
    configs \
    lifecycle \
    resources

multi-process: SUBDIRS += \
    crash/apps/tld.test.crash/terminator2 \
    crash \
    processtitle
