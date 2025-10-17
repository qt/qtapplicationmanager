
qt_internal_add_test(tst_systemreader
    EXCEPTIONS
    SOURCES
        ../error-checking.h
        tst_systemreader.cpp
    DEFINES
        QTEST_THROW_ON_FAIL
        QTEST_THROW_ON_SKIP
    LIBRARIES
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManManagerPrivate
        Qt::AppManMonitorPrivate
        Qt::AppManWindowPrivate
    BUILTIN_TESTDATA
    TESTDATA
        root/proc/1234/cgroup
        root/sys/fs/cgroup/memory/system.slice/run-u5853.scope/memory.limit_in_bytes
        root/sys/fs/cgroup/memory/system.slice/run-u5853.scope/memory.stat

)
