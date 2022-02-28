
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

if(TARGET ${target_name} AND NOT QT_FEATURE_cross_compile)
    set(qml_install_dir "${INSTALL_QMLDIR}")
    qt_path_join(qml_install_dir ${QT_INSTALL_DIR} ${qml_install_dir})

    if(QT_WILL_INSTALL)
        qt_install(
            DIRECTORY "${CMAKE_SOURCE_DIR}/qmltypes/QtApplicationManager"
            DESTINATION "${qml_install_dir}"
        )
    else()
        set(COPY_COMMAND COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/qmltypes" "${qml_install_dir}")
    endif()

    qt_internal_collect_command_environment(env_path env_plugin_path)

    # We have to set the env to get the PATH set to find the Qt libraries
    # needed by the appman-dumpqmltypes binary (we cannot guarantee that the
    # target is in $QTDIR/bin)
    # In addition $<TARGET_FILE:...> will result in a broken path to the
    # tool in a multi-config build, because Qt only builds the tools once.
    # Hence we need to construct the path to the tool ourselves (copied from
    # QtToolHelpers.cmake)

    set(tool_path "${QT_BUILD_DIR}/${INSTALL_BINDIR}/appman-dumpqmltypes")

    add_custom_command(
        TARGET ${target_name}
        POST_BUILD
        BYPRODUCTS "qmltypes"
        COMMAND "${CMAKE_COMMAND}" -E env "PATH=${env_path}${QT_PATH_SEPARATOR}$ENV{PATH}"
                "${tool_path}" "${CMAKE_SOURCE_DIR}/qmltypes"
        ${COPY_COMMAND}
        DEPENDS "${tool_path}"
        COMMENT "Generating qmltypes using ${tool_path}"
        VERBATIM
    )
endif()
