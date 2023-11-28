
qt_find_package(WrapLibYaml PROVIDED_TARGETS WrapLibYaml::WrapLibYaml)
if(TARGET WrapBacktrace::WrapBacktrace)
    qt_internal_disable_find_package_global_promotion(WrapBacktrace::WrapBacktrace)
endif()
qt_find_package(WrapBacktrace PROVIDED_TARGETS WrapBacktrace::WrapBacktrace)

# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManCommonPrivate
    CONFIG_MODULE_NAME appman_common
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        architecture.cpp architecture.h
        colorprint.h colorprint.cpp
        configcache.cpp configcache.h configcache_p.h
        console.h console.cpp
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
        unixsignalhandler.cpp unixsignalhandler.h
        utilities.cpp utilities.h
    LIBRARIES
        Qt::CorePrivate
        Qt::QmlPrivate
    PUBLIC_LIBRARIES
        Qt::Concurrent
        Qt::Core
        Qt::Network
        Qt::Qml
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

qt_internal_extend_target(AppManCommonPrivate CONDITION TARGET WrapBacktrace::WrapBacktrace
    PUBLIC_LIBRARIES
        WrapBacktrace::WrapBacktrace
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
