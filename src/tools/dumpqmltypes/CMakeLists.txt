
qt_get_tool_target_name(target_name appman-dumpqmltypes)
qt_internal_add_tool(${target_name}
    EXCEPTIONS
    TOOLS_TARGET AppManMainPrivate
    SOURCES
        dumpqmltypes.cpp
    PUBLIC_LIBRARIES
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManIntentClientPrivate
        Qt::AppManIntentServerPrivate
        Qt::AppManLauncherPrivate
        Qt::AppManMainPrivate
        Qt::AppManManagerPrivate
        Qt::AppManMonitorPrivate
        Qt::AppManNotificationPrivate
        Qt::AppManSharedMainPrivate
        Qt::AppManWindowPrivate
)

#### Keys ignored in scope 3:.:.:dumpqmltypes.pro:build_pass OR NOT debug_and_release:
# INSTALLS = "qmltypes_file"
# QMAKE_POST_LINK = "$$QMLPLUGINDUMP" "$$SOURCE_DIR/qmltypes"
# qmltypes_file.CONFIG = "no_check_exist" "directory"
# qmltypes_file.files = "$$SOURCE_DIR/qmltypes/QtApplicationManager"
# qmltypes_file.path = "$$[QT_INSTALL_QML]"
