
qt_feature_evaluate_features("${CMAKE_CURRENT_SOURCE_DIR}/common-lib/configure.cmake")
qt_feature_evaluate_features("${CMAKE_CURRENT_SOURCE_DIR}/package-lib/configure.cmake")
qt_feature_evaluate_features("${CMAKE_CURRENT_SOURCE_DIR}/window-lib/configure.cmake")

add_subdirectory(3rdparty)
add_subdirectory(common-lib)
add_subdirectory(application-lib)

if(QT_FEATURE_am_installer)
    add_subdirectory(crypto-lib)
    add_subdirectory(package-lib)
endif()

if(NOT QT_FEATURE_am_tools_only)
    add_subdirectory(plugin-interfaces)
    add_subdirectory(notification-lib)
    add_subdirectory(monitor-lib)
    add_subdirectory(shared-main-lib)
    add_subdirectory(intent-server-lib)
    add_subdirectory(intent-client-lib)
    add_subdirectory(manager-lib)
    add_subdirectory(window-lib)

    if(TARGET Qt::DBus AND QT_FEATURE_am_external_dbus_interfaces)
        add_subdirectory(dbus-lib)
    endif()

    add_subdirectory(main-lib)
    add_subdirectory(tools/appman)
    add_subdirectory(tools/testrunner)

    if(TARGET Qt::DBus)
        add_subdirectory(launcher-lib)
        add_subdirectory(tools/controller)

        # This tool links against everything to extract the Qml type information
        if (QT_FEATURE_am_installer)
            add_subdirectory(tools/dumpqmltypes)
        endif()

        if (QT_FEATURE_am_multi_process)
            add_subdirectory(tools/launcher-qml)
        endif()
    endif()

    if(QT_FEATURE_am_package_server)
        add_subdirectory(tools/package-server)
    endif()
endif()
add_subdirectory(plugins)

if(NOT QT_FEATURE_cross_compile OR QT_FEATURE_am_tools_only)
    if (QT_FEATURE_am_installer)
        add_subdirectory(tools/packager)
    endif()
endif()
