
# All SocketIpc* classes are Q_AUTOTEST_EXPORT only
if(NOT QT_FEATURE_private_tests)
    return()
endif()

qt_internal_add_test(tst_socketipc
    EXCEPTIONS
    SOURCES
        tst_socketipc.cpp
    DEFINES
        QTEST_THROW_ON_FAIL
        QTEST_THROW_ON_SKIP
    LIBRARIES
        Qt::AppManCommonPrivate
        Qt::AppManSystemUIPrivate
)
