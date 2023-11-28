cmake_minimum_required(VERSION 3.16)
project(application-features LANGUAGES CXX)

find_package(Qt6 COMPONENTS AppManMainPrivate)
qt_standard_project_setup()

if(NOT DEFINED INSTALL_EXAMPLESDIR)
    set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/application-features")

qt6_am_add_systemui_wrapper(
    application-features
    INSTALL_DIR "${INSTALL_EXAMPLEDIR}"
    CONFIG_YAML am-config.yaml
    EXTRA_FILES
        client.qml
        apps/Crash/info.yaml
        apps/Crash/icon.png
        apps/Compositor
        apps/Twins
        apps/Widgets
)

add_subdirectory(SystemUi)
add_subdirectory(apps/Crash)
add_subdirectory(apps/Widgets)
