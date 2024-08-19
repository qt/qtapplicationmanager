cmake_minimum_required(VERSION 3.16)
project(intents LANGUAGES CXX)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/intents")


find_package(Qt6 COMPONENTS AppManMainPrivate)

qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

## System-UI ##

qt_add_executable(intents WIN32
    main.cpp
)

# This define flags us as an "appman" and allows us to link against the AppMan's private libraries
target_compile_definitions(intents PRIVATE AM_COMPILING_APPMAN)

qt_add_qml_module(intents
    URI SystemUI
    QML_FILES
        system-ui.qml
    RESOURCES
        am-config.yaml
)

qt_add_qml_module(shared
    URI shared
    QML_FILES
        shared/IntentsApplicationWindow.qml
        shared/IntentsUIPage.qml
    PLUGIN_TARGET shared
    OUTPUT_DIRECTORY
        imports/shared
)

target_link_libraries(intents PRIVATE
    Qt::AppManMainPrivate
)

## Apps ##


qt6_am_create_builtin_package(intents.red
    OUTPUT_DIRECTORY apps/intents.red
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/intents.red
    FILES
        apps/intents.red/info.yaml
        apps/intents.red/icon.png
        apps/intents.red/main.qml
)

qt6_am_create_builtin_package(intents.blue
    OUTPUT_DIRECTORY apps/intents.blue
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/intents.blue
    FILES
        apps/intents.blue/info.yaml
        apps/intents.blue/icon.png
        apps/intents.blue/main.qml
)

qt6_am_create_builtin_package(intents.green
    OUTPUT_DIRECTORY apps/intents.green
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/intents.green
    FILES
        apps/intents.green/info.yaml
        apps/intents.green/icon.png
        apps/intents.green/main.qml
)

install(TARGETS intents
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)

install(TARGETS shared
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}/imports/shared"
)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/imports/shared/qmldir DESTINATION "${INSTALL_EXAMPLEDIR}/imports/shared")
