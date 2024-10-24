
qt_feature_evaluate_features("${CMAKE_CURRENT_SOURCE_DIR}/common-lib/configure.cmake")

set(QT_SBOM_DEFAULT_QT_LICENSE_ID_LIBRARIES "QT_COMMERCIAL_OR_GPL3")

# Workaround for boot2qt to successfully build, until someone can figure out
# why it doesn't like the tools from being excluded.
if(NOT QT_APPMAN_SKIP_EXCLUDE_TOOLS_FROM_DEFAULT_TARGET)
    qt_exclude_tool_directories_from_default_target(
        tools/appman
        tools/testrunner
        tools/controller
        tools/launcher-qml
        tools/package-server
    )
endif()

add_subdirectory(3rdparty)
add_subdirectory(wayland-extensions)
add_subdirectory(dbus-interfaces)
add_subdirectory(common-lib)
add_subdirectory(application-lib)

if(QT_FEATURE_am_installer)
    add_subdirectory(crypto-lib)
    add_subdirectory(package-lib)
endif()

if(NOT QT_FEATURE_am_tools_only)
    if(TARGET Qt::DBus)
        add_subdirectory(dbus-lib)
    endif()
    add_subdirectory(plugin-interfaces)
    add_subdirectory(monitor-lib)
    add_subdirectory(shared-main-lib)
    add_subdirectory(intent-server-lib)
    add_subdirectory(intent-client-lib)
    add_subdirectory(manager-lib)
    add_subdirectory(window-lib)
    add_subdirectory(plugins)
    add_subdirectory(qml)
    add_subdirectory(main-lib)
    add_subdirectory(tools/appman)
    add_subdirectory(tools/testrunner)

    if(TARGET Qt::DBus)
        add_subdirectory(application-main-lib)
        add_subdirectory(tools/controller)

        if (QT_FEATURE_am_multi_process)
            add_subdirectory(tools/launcher-qml)
        endif()
    endif()

    if(QT_FEATURE_am_package_server)
        add_subdirectory(tools/package-server)
    endif()
endif()

if (QT_FEATURE_am_installer)
    add_subdirectory(tools/packager)
endif()
