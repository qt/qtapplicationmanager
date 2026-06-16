
qt_internal_add_test(tst_installationreport
    EXCEPTIONS
    SOURCES
        ../error-checking.h
        tst_installationreport.cpp
    DEFINES
        QTEST_THROW_ON_FAIL
        QTEST_THROW_ON_SKIP
    LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
        Qt::AppManPackagePrivate
    BUILTIN_TESTDATA
    TESTDATA
        legacy-installation-report-v3.yaml
)
