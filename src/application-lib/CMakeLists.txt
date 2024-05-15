
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
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Network
        Qt::AppManCommonPrivate
    DEFINES
        AM_COMPILING_APPMAN
)
