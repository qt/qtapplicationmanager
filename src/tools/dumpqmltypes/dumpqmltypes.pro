requires(!headless)

TEMPLATE = app
TARGET   = appman-dumpqmltypes

load(am-config)

QT = core
QT *= \
    appman_common-private \
    appman_application-private \
    appman_manager-private \
    appman_installer-private \
    appman_notification-private \
    appman_window-private \
    appman_launcher-private \
    appman_intent_client-private \
    appman_intent_server-private \
    appman_monitor-private \
    appman_shared_main-private \

CONFIG *= console

SOURCES += \
    dumpqmltypes.cpp

load(qt_tool)

load(install-prefix)


!cross_compile {
    qtPrepareTool(QMLPLUGINDUMP, appman-dumpqmltypes)
    QT_TOOL_ENV =

    build_pass|!debug_and_release {
        QMAKE_POST_LINK += $$QMLPLUGINDUMP $$OUT_PWD
        qmltypes_file.files = $$OUT_PWD/QtApplicationManager
        qmltypes_file.path = $$[QT_INSTALL_QML]
        qmltypes_file.CONFIG = no_check_exist directory

        INSTALLS += qmltypes_file
    }
}
