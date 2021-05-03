# Generated from package-lib.pro.

#####################################################################
## AppManPackage Module:
#####################################################################

qt_find_package(WrapLibArchive PROVIDED_TARGETS WrapLibArchive::WrapLibArchive)

# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManPackagePrivate
    CONFIG_MODULE_NAME appman_package
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        packagecreator.cpp packagecreator.h packagecreator_p.h
        packageextractor.cpp packageextractor.h packageextractor_p.h
        packageutilities.cpp packageutilities.h packageutilities_p.h
    LIBRARIES
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Network
)

qt_internal_extend_target(AppManPackagePrivate CONDITION QT_FEATURE_system_libarchive
    LIBRARIES
        WrapLibArchive::WrapLibArchive
)

qt_internal_extend_target(AppManPackagePrivate CONDITION NOT QT_FEATURE_system_libarchive
    LIBRARIES
        Qt::BundledLibArchive
)
