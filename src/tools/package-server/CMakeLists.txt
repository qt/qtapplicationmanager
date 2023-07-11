
qt_internal_add_app(appman-package-server
    EXCEPTIONS
    SOURCES
        package-server.cpp package-server.h
        psconfiguration.h psconfiguration.cpp
        pspackages.h pspackages.cpp pspackages_p.h
        pshttpinterface.h pshttpinterface.cpp pshttpinterface_p.h
    LIBRARIES
        Qt::Network
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManCryptoPrivate
        Qt::AppManPackagePrivate
        Qt::HttpServer
    DEFINES
        AM_COMPILING_APPMAN
)
