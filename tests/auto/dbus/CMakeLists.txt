
qt_internal_add_test(tst_dbus
    EXCEPTIONS
    SOURCES
        tst_dbus.cpp
    LIBRARIES
        Qt::DBus
        Qt::Network
        Qt::AppManCommonPrivate
        Qt::AppManPackagePrivate
        Qt::AppManSystemUIPrivate
    DEFINES
        QTEST_THROW_ON_FAIL
        QTEST_THROW_ON_SKIP
        AM_TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/../../data/"
    BUILTIN_TESTDATA
)

add_subdirectory(apps/notifier)
