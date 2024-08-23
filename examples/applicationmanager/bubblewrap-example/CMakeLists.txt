cmake_minimum_required(VERSION 3.16)
project(bubblewrap-example LANGUAGES CXX)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/bubblewrap-example")

find_package(Qt6 COMPONENTS AppManMainPrivate)

qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

## System-UI ##

qt_add_executable(bubblewrap-example WIN32
    main.cpp
)

# This define flags us as an "appman" and allows us to link against the AppMan's private libraries
target_compile_definitions(bubblewrap-example PRIVATE AM_COMPILING_APPMAN)

qt_add_qml_module(bubblewrap-example
    URI SystemUI
    QML_FILES
        system-ui.qml
    RESOURCES
        am-config.yaml
)

qt_add_qml_module(networkhelper
    URI "NetworkHelper"
    SOURCES
        networkhelper.cpp networkhelper.h
    PLUGIN_TARGET networkhelper
    OUTPUT_DIRECTORY
        imports/NetworkHelper
)

target_link_libraries(bubblewrap-example PRIVATE
    Qt::AppManMainPrivate
)

## Apps ##


qt_am_create_builtin_package(bubblewrap.browser
    OUTPUT_DIRECTORY apps/bubblewrap.browser
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/bubblewrap.browser
    FILES
        apps/bubblewrap.browser/info.yaml
        apps/bubblewrap.browser/icon.svg
        apps/bubblewrap.browser/main.qml
        apps/bubblewrap.browser/Browser.qml
)

qt_am_create_builtin_package(bubblewrap.networkinfo
    OUTPUT_DIRECTORY apps/bubblewrap.networkinfo
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/bubblewrap.networkinfo
    FILES
        apps/bubblewrap.networkinfo/info.yaml
        apps/bubblewrap.networkinfo/icon.svg
        apps/bubblewrap.networkinfo/main.qml
)

qt_am_create_builtin_package(bubblewrap.simple
    OUTPUT_DIRECTORY apps/bubblewrap.simple
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/bubblewrap.simple
    FILES
        apps/bubblewrap.simple/info.yaml
        apps/bubblewrap.simple/icon.svg
        apps/bubblewrap.simple/main.qml
)

file(COPY bubblewrap-network-setup.sh DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

install(TARGETS bubblewrap-example
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)

install(TARGETS networkhelper
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}/imports/NetworkHelper"
)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/imports/NetworkHelper/qmldir DESTINATION "${INSTALL_EXAMPLEDIR}/imports/NetworkHelper")

install(FILES bubblewrap-network-setup.sh
    DESTINATION "${INSTALL_EXAMPLEDIR}"
)
