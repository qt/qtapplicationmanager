
qt_internal_add_qml_module(AppManQmlPrivate
    CONFIG_MODULE_NAME appman_qml
    URI QtApplicationManager
    VERSION "${PROJECT_VERSION}"
    NO_PLUGIN
    STATIC
    EXCEPTIONS
    SOURCES
        qtapplicationmanager_p.h
    DEPENDENCIES
        QtQuick
    LIBRARIES
        Qt::AppManSharedMainPrivate
        Qt::AppManIntentClientPrivate
    DEFINES
        AM_COMPILING_APPMAN
)

qt_internal_add_qml_module(AppManQmlSystemUIPrivate
    CONFIG_MODULE_NAME appman_qml_systemui
    URI QtApplicationManager.SystemUI
    VERSION "${PROJECT_VERSION}"
    NO_PLUGIN
    STATIC
    EXCEPTIONS
    SOURCES
        qtapplicationmanager_systemui_p.h
    DEPENDENCIES
        QtQuick
    IMPORTS
        QtApplicationManager/auto
        QtQuick
    LIBRARIES
        Qt::AppManManagerPrivate
        Qt::AppManWindowPrivate
        Qt::AppManIntentServerPrivate
        Qt::AppManQmlPrivate
    DEFINES
        AM_COMPILING_APPMAN
)

qt_internal_add_qml_module(AppManQmlApplicationPrivate
    CONFIG_MODULE_NAME appman_qml_application
    URI QtApplicationManager.Application
    VERSION "${PROJECT_VERSION}"
    NO_PLUGIN
    STATIC
    EXCEPTIONS
    SOURCES
        qtapplicationmanager_application_p.h
    DEPENDENCIES
        QtQuick
    IMPORTS
        QtApplicationManager/auto
        QtQuick
    LIBRARIES
        Qt::AppManSharedMainPrivate
        Qt::AppManIntentClientPrivate
        Qt::AppManQmlPrivate
    DEFINES
        AM_COMPILING_APPMAN
)
