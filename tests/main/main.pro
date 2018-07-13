TARGET = tst_main

CONFIG += insignificant_test # no test-data available in the CI

include($$PWD/../tests.pri)

QT *= appman_manager-private \
      appman_application-private \
      appman_common-private \
      appman_main-private \

SOURCES += tst_main.cpp
