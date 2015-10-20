
TEMPLATE = subdirs

include($$BASE_PRI)

SUBDIRS = \
    application \
    runtime \
    cryptography \
    digestfilter \
    cipherfilter \
    signature \
    utilities \
    packagecreator \
    packageextractor \
    installationreport \
    applicationinstaller \
    packager-tool \

linux*:SUBDIRS += \
    sudo \

OTHER_FILES += \
    tests.pri \
    data/create-test-packages.sh \
    data/certificates/create-test-certificates.sh \
