
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
    packager-tool \
    installationreport

## disabled temporarily until we can get them to work in the CI
# applicationinstaller
#
#linux*:SUBDIRS += \
#    sudo \

OTHER_FILES += \
    tests.pri \
    data/create-test-packages.sh \
    data/certificates/create-test-certificates.sh \

# create test data on the fly - this is needed for the CI server
testdata.target = testdata
testdata.depends = $$PWD/data/create-test-packages.sh $$BUILD_DIR/bin/appman-packager
testdata.commands = (cd $$PWD/data ; ./create-test-packages.sh $$BUILD_DIR/bin/appman-packager)
QMAKE_EXTRA_TARGETS += testdata

# qmake would create a default check target implicitly, but since we need 'testdata' as an
# dependency, we have to set it up explicitly
prepareRecursiveTarget(check)
check.depends = testdata $$check.depends
QMAKE_EXTRA_TARGETS *= check
