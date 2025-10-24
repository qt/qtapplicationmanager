
qt_internal_add_module(AppManCryptoPrivate
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        qtappmancryptoglobal.h
        certificate.h certificate.cpp
        signature.cpp signature.h signature_p.h
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::AppManCommonPrivate
    DEFINES
        AM_COMPILING_APPMAN
)

qt_internal_extend_target(AppManCryptoPrivate CONDITION WIN32
    SOURCES
        signature_win.cpp
    PUBLIC_LIBRARIES
        advapi32
        crypt32
)

qt_internal_extend_target(AppManCryptoPrivate CONDITION UNIX
    SOURCES
        libcryptofunction.cpp libcryptofunction.h
        signature_openssl.cpp
    PUBLIC_DEFINES
        QT_AM_USE_LIBCRYPTO
    PUBLIC_LIBRARIES
        Qt::Network
)
