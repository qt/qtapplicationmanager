
qt_feature_evaluate_features("${CMAKE_CURRENT_SOURCE_DIR}/common-lib/configure.cmake")

set(QT_SBOM_DEFAULT_QT_LICENSE_ID_LIBRARIES "QT_COMMERCIAL_OR_GPL3")

if(NOT QT_FEATURE_system_zlib)
    find_package(Qt6 COMPONENTS ZlibPrivate)
elseif(NOT TARGET WrapZLIB::WrapZLIB)
    qt_find_package(WrapZLIB PROVIDED_TARGETS WrapZLIB::WrapZLIB)
endif()

add_subdirectory(3rdparty)
add_subdirectory(wayland-extensions)
add_subdirectory(dbus-interfaces)
add_subdirectory(common-lib)
add_subdirectory(package-lib)

if(NOT QT_FEATURE_am_tools_only)
    add_subdirectory(plugin-interfaces)
    add_subdirectory(shared-lib)
    add_subdirectory(plugins)
    add_subdirectory(systemui-lib)
    add_subdirectory(tools/appman)
    add_subdirectory(tools/testrunner)

    if(TARGET Qt::DBus)
        add_subdirectory(application-lib)
        add_subdirectory(tools/controller)

        if(QT_FEATURE_am_multi_process)
            add_subdirectory(tools/launcher-qml)
        endif()
    endif()
endif()

if(QT_FEATURE_am_installer)
    add_subdirectory(tools/packager)
endif()

if(QT_FEATURE_am_package_server)
    add_subdirectory(tools/package-server)
endif()
