cmake_minimum_required(VERSION 3.16)
project(launch-intents LANGUAGES CXX)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/launch-intents")


find_package(Qt6 COMPONENTS AppManMainPrivate)

qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

## System-UI ##

qt_add_executable(launch-intents WIN32
    main.cpp
)

# This define flags us as an "appman" and allows us to link against the AppMan's private libraries
target_compile_definitions(launch-intents PRIVATE AM_COMPILING_APPMAN)

qt_add_qml_module(launch-intents
    URI SystemUI
    QML_FILES
        system-ui.qml
    RESOURCES
        am-config.yaml
)

target_link_libraries(launch-intents PRIVATE
    Qt::AppManMainPrivate
)

## Apps ##

qt6_am_create_builtin_package(launch-intents.red
    OUTPUT_DIRECTORY apps/launch-intents.red
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/launch-intents.red
    FILES
        apps/launch-intents.red/info.yaml
        apps/launch-intents.red/icon.png
        apps/launch-intents.red/main.qml
)

qt6_am_create_builtin_package(launch-intents.blue
    OUTPUT_DIRECTORY apps/launch-intents.blue
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/launch-intents.blue
    FILES
        apps/launch-intents.blue/info.yaml
        apps/launch-intents.blue/icon.png
        apps/launch-intents.blue/main.qml
)

qt6_am_create_builtin_package(launch-intents.green
    OUTPUT_DIRECTORY apps/launch-intents.green
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/launch-intents.green
    FILES
        apps/launch-intents.green/info.yaml
        apps/launch-intents.green/icon.png
        apps/launch-intents.green/main.qml
)

install(TARGETS launch-intents
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
)
