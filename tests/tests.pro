TEMPLATE = subdirs

load(am-config)
requires(!disable-installer)

SUBDIRS = \
    manual \
    application \
    applicationinfo \
    main \
    runtime \
    cryptography \
    signature \
    utilities \
    installationreport \
    packagecreator \
    packageextractor \
    packager-tool \
    applicationinstaller \
    debugwrapper \
    qml \
    yaml \
    configuration \

linux*:SUBDIRS += \
    sudo \
    processreader \
    systemreader \

OTHER_FILES += \
    tests.pri \
    data/create-test-packages.sh \
    data/certificates/create-test-certificates.sh \
    data/utilities.sh \

# sadly, the appman-packager is too complex to build as a host tool
!cross_compile {
    prepareRecursiveTarget(check)
    qtPrepareTool(APPMAN_PACKAGER, appman-packager)

    unix {
        # create test data on the fly - this is needed for the CI server
        testdata.target = testdata
        testdata.depends = $$PWD/data/create-test-packages.sh $$APPMAN_PACKAGER_EXE
        testdata.commands = (cd $$PWD/data ; ./create-test-packages.sh $$APPMAN_PACKAGER)
        QMAKE_EXTRA_TARGETS += testdata

        # qmake would create a default check target implicitly, but since we need 'testdata' as an
        # dependency, we have to set it up explicitly
        check.depends = testdata $$check.depends

        # add the first target to make sure the testdata is created already at the compile step
        first.depends = testdata
        QMAKE_EXTRA_TARGETS *= first
    }
    QMAKE_EXTRA_TARGETS *= check
}
