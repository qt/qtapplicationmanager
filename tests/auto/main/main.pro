TARGET = tst_main

include($$PWD/../tests.pri)

# this test keeps crashing in a VirtualBox based CI environment, when executed
# via ssh (which gets the test run in Window's session 0 (no visible desktop)
luxoft-ci:CONFIG *= insignificant_test

QT *= appman_manager-private \
      appman_application-private \
      appman_common-private \
      appman_main-private \
      appman_intent_server-private \

SOURCES += tst_main.cpp

RESOURCES = main.qrc

OTHER_FILES += am-config.yaml

