# Generated from notification-lib.pro.

#####################################################################
## AppManNotification Module:
#####################################################################

# temporary hack to get around the "#pragma once not allowed in cpp" error
set(QT_FEATURE_headersclean FALSE)

qt_internal_add_module(AppManNotificationPrivate
    CONFIG_MODULE_NAME appman_notification
    STATIC
    EXCEPTIONS
    INTERNAL_MODULE
    SOURCES
        notification.cpp notification.h
    LIBRARIES
        Qt::AppManCommonPrivate
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Qml
)
