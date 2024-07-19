cmake_minimum_required(VERSION 3.16)
project(process-status LANGUAGES CXX)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/process-status")


find_package(Qt6 COMPONENTS AppManMainPrivate)

qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

## System-UI ##

qt_add_executable(process-status WIN32
    main.cpp
)

# This define flags us as an "appman" and allows us to link against the AppMan's private libraries
target_compile_definitions(process-status PRIVATE AM_COMPILING_APPMAN)

qt_add_qml_module(process-status
    URI SystemUI
    QML_FILES
        system-ui/main.qml
        system-ui/ApplicationDisplay.qml
        system-ui/CpuGraph.qml
        system-ui/MemoryText.qml
        system-ui/Stats.qml
    RESOURCES
        am-config.yaml
)

target_link_libraries(process-status PRIVATE
    Qt::AppManMainPrivate
)

## Apps ##

qt6_am_create_builtin_package(process-status.cpu-hog
    OUTPUT_DIRECTORY apps/process-status.cpu-hog
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/process-status.cpu-hog
    FILES
        apps/process-status.cpu-hog/info.yaml
        apps/process-status.cpu-hog/icon.png
        apps/process-status.cpu-hog/main.qml
)

qt6_am_create_builtin_package(process-status.mem-hog
    OUTPUT_DIRECTORY apps/process-status.mem-hog
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/process-status.mem-hog
    FILES
        apps/process-status.mem-hog/info.yaml
        apps/process-status.mem-hog/icon.png
        apps/process-status.mem-hog/main.qml
)

qt6_am_create_builtin_package(process-status.slim
    OUTPUT_DIRECTORY apps/process-status.slim
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/process-status.slim
    FILES
        apps/process-status.slim/info.yaml
        apps/process-status.slim/icon.png
        apps/process-status.slim/main.qml
)

install(TARGETS process-status
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
)
