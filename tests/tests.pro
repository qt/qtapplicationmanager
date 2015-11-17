
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
    installationreport

#linux*:SUBDIRS += \
#    sudo \

# packageextractor kept back due to missing test assets
# applicationinstaller
# packager-tool

OTHER_FILES += \
    tests.pri \
    data/create-test-packages.sh \
    data/certificates/create-test-certificates.sh \
