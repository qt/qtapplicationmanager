
qt_internal_add_test(tst_filesystemmountwatcher
    EXCEPTIONS
    SOURCES
        tst_filesystemmountwatcher.cpp
    DEFINES
        QTEST_THROW_ON_FAIL
        QTEST_THROW_ON_SKIP
    LIBRARIES
        Qt::AppManCommonPrivate
)
