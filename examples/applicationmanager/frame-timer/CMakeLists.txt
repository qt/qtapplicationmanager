cmake_minimum_required(VERSION 3.16)
project(frame-timer LANGUAGES CXX)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/frame-timer")

find_package(Qt6 COMPONENTS AppManMainPrivate)

qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

## System-UI ##

qt_add_executable(frame-timer WIN32
    main.cpp
)

# This define flags us as an "appman" and allows us to link against the AppMan's private libraries
target_compile_definitions(frame-timer PRIVATE AM_COMPILING_APPMAN)

qt_add_qml_module(frame-timer
    URI SystemUI
    QML_FILES
        system-ui/main.qml
    RESOURCES
        am-config.yaml
)

target_link_libraries(frame-timer PRIVATE
    Qt::AppManMainPrivate
)

## Apps ##

qt6_am_create_builtin_package(frame-timer.fish
    OUTPUT_DIRECTORY apps/frame-timer.fish
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/frame-timer.fish
    FILES
        apps/frame-timer.fish/info.yaml
        apps/frame-timer.fish/fish.svg
        apps/frame-timer.fish/fish.qml
)

qt6_am_create_builtin_package(frame-timer.rabbit
    OUTPUT_DIRECTORY apps/frame-timer.rabbit
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/frame-timer.rabbit
    FILES
        apps/frame-timer.rabbit/info.yaml
        apps/frame-timer.rabbit/rabbit.svg
        apps/frame-timer.rabbit/rabbit.qml
)

install(TARGETS frame-timer
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
)
