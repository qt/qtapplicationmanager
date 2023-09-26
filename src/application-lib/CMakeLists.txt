
# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManApplicationPrivate
    CONFIG_MODULE_NAME appman_application
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        applicationinfo.cpp applicationinfo.h
        installationreport.cpp installationreport.h
        intentinfo.cpp intentinfo.h
        packagedatabase.cpp packagedatabase.h
        packageinfo.cpp packageinfo.h
        packagescanner.h
        yamlpackagescanner.cpp yamlpackagescanner.h
    LIBRARIES
        Qt::AppManCommonPrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Network
    DEFINES
        AM_COMPILING_APPMAN
)
