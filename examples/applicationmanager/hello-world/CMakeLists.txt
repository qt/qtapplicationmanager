cmake_minimum_required(VERSION 3.16)
project(hello-world LANGUAGES CXX)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/hello-world")


find_package(Qt6 COMPONENTS AppManMainPrivate)

qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

## System-UI ##

qt_add_executable(hello-world WIN32
    main.cpp
)

# This define flags us as an "appman" and allows us to link against the AppMan's private libraries
target_compile_definitions(hello-world PRIVATE AM_COMPILING_APPMAN)

qt_add_qml_module(hello-world
    URI SystemUI
    QML_FILES
        system-ui.qml
    RESOURCES
        am-config.yaml
)

target_link_libraries(hello-world PRIVATE
    Qt::AppManMainPrivate
)

## Apps ##

qt6_am_create_builtin_package(hello-world.blue
    OUTPUT_DIRECTORY apps/hello-world.blue
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/hello-world.blue
    FILES
        apps/hello-world.blue/info.yaml
        apps/hello-world.blue/icon.png
        apps/hello-world.blue/main.qml
)

qt6_am_create_builtin_package(hello-world.green
    OUTPUT_DIRECTORY apps/hello-world.green
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/hello-world.green
    FILES
        apps/hello-world.green/info.yaml
        apps/hello-world.green/icon.png
        apps/hello-world.green/main.qml
)

qt6_am_create_builtin_package(hello-world.red
    OUTPUT_DIRECTORY apps/hello-world.red
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/hello-world.red
    FILES
        apps/hello-world.red/info.yaml
        apps/hello-world.red/icon.png
        apps/hello-world.red/main.qml
)

install(TARGETS hello-world
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
)
