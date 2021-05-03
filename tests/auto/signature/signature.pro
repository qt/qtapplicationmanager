TARGET = tst_signature

include($$PWD/../tests.pri)

QT *= appman_common-private appman_crypto-private

SOURCES += tst_signature.cpp

OTHER_FILES += create-test-data.sh

RESOURCES += tst_signature.qrc
