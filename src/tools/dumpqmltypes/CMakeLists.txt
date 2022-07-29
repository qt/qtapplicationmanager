
qt_internal_add_app(appman-dumpqmltypes
    EXCEPTIONS
    SOURCES
        dumpqmltypes.cpp
    LIBRARIES
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
    DEFINES
        AM_COMPILING_APPMAN
)

# regenerate the qmltypes, if possible
if(NOT QT_FEATURE_cross_compile)
    qt_internal_collect_command_environment(env_path env_plugin_path)

    # We have to set the env to get the PATH set to find the Qt libraries
    # needed by the appman-dumpqmltypes binary (we cannot guarantee that the
    # target is in $QTDIR/bin)

    add_custom_command(
        TARGET appman-dumpqmltypes
        POST_BUILD
        BYPRODUCTS "qmltypes"
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${env_path}${QT_PATH_SEPARATOR}$ENV{PATH}"
                "$<TARGET_FILE:appman-dumpqmltypes>" "${CMAKE_SOURCE_DIR}/qmltypes"
        COMMENT "Generating qmltypes using appman-dumpqmltypes"
        VERBATIM
    )
endif()

# install the qmltypes
set(qml_install_dir "${INSTALL_QMLDIR}")
qt_path_join(qml_install_dir ${QT_INSTALL_DIR} ${qml_install_dir})

if(QT_WILL_INSTALL)
    qt_install(
        DIRECTORY "${CMAKE_SOURCE_DIR}/qmltypes/QtApplicationManager"
        DESTINATION "${qml_install_dir}"
    )
else()
    add_custom_command(
        TARGET appman-dumpqmltypes
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/qmltypes" "${qml_install_dir}"
        COMMENT "Copying qmltypes to Qt directory"
        VERBATIM
    )
endif()
