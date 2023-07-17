
qt_find_package(WrapLibYaml PROVIDED_TARGETS WrapLibYaml::WrapLibYaml)

# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManCommonPrivate
    CONFIG_MODULE_NAME appman_common
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        configcache.cpp configcache.h configcache_p.h
        crashhandler.cpp crashhandler.h
        dbus-utilities.cpp dbus-utilities.h
        error.h
        exception.cpp exception.h
        filesystemmountwatcher.cpp filesystemmountwatcher.h
        global.h
        logging.cpp logging.h
        processtitle.cpp processtitle.h
        qml-utilities.cpp qml-utilities.h
        qtyaml.cpp qtyaml.h
        startuptimer.cpp startuptimer.h
        unixsignalhandler.cpp unixsignalhandler.h
        utilities.cpp utilities.h
    PUBLIC_LIBRARIES
        Qt::Concurrent
        Qt::Core
        Qt::CorePrivate
        Qt::Network
        Qt::Qml
        Qt::QmlPrivate
    DEFINES
        AM_COMPILING_APPMAN
)

qt_internal_extend_target(AppManCommonPrivate CONDITION TARGET Qt::DltLogging
    PUBLIC_LIBRARIES
        Qt::DltLogging
)

qt_internal_extend_target(AppManCommonPrivate CONDITION TARGET Qt::DBus
    PUBLIC_LIBRARIES
        Qt::DBus
)

qt_internal_extend_target(AppManCommonPrivate CONDITION LINUX
    PUBLIC_LIBRARIES
        dl
)

qt_internal_extend_target(AppManCommonPrivate CONDITION QNX
    PUBLIC_LIBRARIES
        backtrace
)

qt_internal_extend_target(AppManCommonPrivate CONDITION QT_FEATURE_am_libbacktrace
    LIBRARIES
        Qt::BundledBacktrace
)

qt_internal_extend_target(AppManCommonPrivate CONDITION QT_FEATURE_am_stackwalker
    LIBRARIES
        Qt::BundledStackwalker
)

qt_internal_extend_target(AppManCommonPrivate CONDITION QT_FEATURE_am_system_libyaml
    LIBRARIES
        WrapLibYaml::WrapLibYaml
)

qt_internal_extend_target(AppManCommonPrivate CONDITION NOT QT_FEATURE_am_system_libyaml
    LIBRARIES
        Qt::BundledLibYaml
)

qt_internal_extend_target(AppManCommonPrivate CONDITION AM_COVERAGE
    LIBRARIES
        gcov
)
