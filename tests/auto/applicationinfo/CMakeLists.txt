
qt_internal_add_test(tst_applicationinfo
    EXCEPTIONS
    SOURCES
        ../error-checking.h
        tst_applicationinfo.cpp
    DEFINES
        QTEST_THROW_ON_FAIL
        QTEST_THROW_ON_SKIP
    LIBRARIES
        Qt::Network
        Qt::AppManPackagePrivate
        Qt::AppManCommonPrivate
        Qt::AppManSystemUIPrivate
    BUILTIN_TESTDATA
    TESTDATA
        data/legacy/info.yaml
        data/minimal/info.yaml
        data/inherit/info.yaml
        data/full/info.yaml

)
