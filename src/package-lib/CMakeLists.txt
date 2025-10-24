
qt_find_package(WrapLibArchive PROVIDED_TARGETS WrapLibArchive::WrapLibArchive)

qt_internal_add_module(AppManPackagePrivate
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        qtappmanpackageglobal.h
        applicationinfo.cpp applicationinfo.h
        installationreport.cpp installationreport.h
        intentinfo.cpp intentinfo.h
        packagedatabase.cpp packagedatabase.h
        packageinfo.cpp packageinfo.h
        packagescanner.h
        yamlpackagescanner.cpp yamlpackagescanner.h

        crypto/certificate.h crypto/certificate.cpp
        crypto/signature.cpp crypto/signature.h crypto/signature_p.h
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Network
        Qt::AppManCommonPrivate
    DEFINES
        AM_COMPILING_APPMAN
)

if(QT_FEATURE_am_installer)
    qt_internal_extend_target(AppManPackagePrivate
        SOURCES
            archive/packagecreator.cpp archive/packagecreator.h archive/packagecreator_p.h
            archive/packageextractor.cpp archive/packageextractor.h archive/packageextractor_p.h
            archive/packageutilities.cpp archive/packageutilities.h archive/packageutilities_p.h
    )

    if(QT_FEATURE_am_system_libarchive)
        qt_internal_extend_target(AppManPackagePrivate
            LIBRARIES
                WrapLibArchive::WrapLibArchive
        )
    else()
        qt_internal_extend_target(AppManPackagePrivate
            LIBRARIES
                Qt::BundledLibArchive
        )
    endif()
endif()

qt_internal_extend_target(AppManPackagePrivate CONDITION WIN32
    SOURCES
        crypto/signature_win.cpp
    PUBLIC_LIBRARIES
        advapi32
        crypt32
)

qt_internal_extend_target(AppManPackagePrivate CONDITION UNIX
    SOURCES
        crypto/libcryptofunction.cpp crypto/libcryptofunction.h
        crypto/signature_openssl.cpp
    PUBLIC_DEFINES
        QT_AM_USE_LIBCRYPTO
    PUBLIC_LIBRARIES
        Qt::Network
)
