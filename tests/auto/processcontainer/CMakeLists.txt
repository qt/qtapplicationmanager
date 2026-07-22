
qt_internal_add_test(tst_processcontainer
    EXCEPTIONS
    SOURCES
        tst_processcontainer.cpp
    DEFINES
        QTEST_THROW_ON_FAIL
        QTEST_THROW_ON_SKIP
    LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
        Qt::AppManPackagePrivate
        Qt::AppManSharedPrivate
        Qt::AppManSystemUIPrivate
)
