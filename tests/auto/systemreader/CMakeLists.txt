
qt_internal_add_test(tst_systemreader
    SOURCES
        ../error-checking.h
        tst_systemreader.cpp
    PUBLIC_LIBRARIES
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManManagerPrivate
        Qt::AppManMonitorPrivate
        Qt::AppManWindowPrivate
)

qt_internal_add_resource(tst_systemreader "systemreader_testdata"
    PREFIX
        "/"
    FILES
        "root/proc/1234/cgroup"
        "root/sys/fs/cgroup/memory/system.slice/run-u5853.scope/memory.limit_in_bytes"
        "root/sys/fs/cgroup/memory/system.slice/run-u5853.scope/memory.stat"
)

qt_internal_extend_target(tst_systemreader CONDITION TARGET Qt::DBus
    PUBLIC_LIBRARIES
        Qt::DBus
)
