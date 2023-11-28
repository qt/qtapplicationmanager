
# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManCryptoPrivate
    CONFIG_MODULE_NAME appman_crypto
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        cryptography.cpp cryptography.h
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

qt_internal_extend_target(AppManCryptoPrivate CONDITION MACOS
    SOURCES
        signature_macos.cpp
    LIBRARIES
        Qt::CorePrivate
    PUBLIC_LIBRARIES
        ${FWCoreFoundation}
        ${FWSecurity}
)

qt_internal_extend_target(AppManCryptoPrivate CONDITION (UNIX AND NOT MACOS)
    SOURCES
        libcryptofunction.cpp libcryptofunction.h
        signature_openssl.cpp
    DEFINES
        AM_USE_LIBCRYPTO
    PUBLIC_LIBRARIES
        Qt::Network
)
