cmake_minimum_required(VERSION 3.16)
project(process-status LANGUAGES CXX)

find_package(Qt6 COMPONENTS AppManSystemUIPrivate)

qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

## System-UI ##

qt_add_executable(process-status
    main.cpp
)

if (QT_FEATURE_static)
    # This define flags us as an "appman" and allows us to link against the AppMan's private libraries
    target_compile_definitions(process-status PRIVATE AM_COMPILING_APPMAN)
endif()

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
    Qt::AppManSystemUIPrivate
)

## Apps ##

qt_am_create_builtin_package(process-status.cpu-hog
    OUTPUT_DIRECTORY apps/process-status.cpu-hog
    INSTALL_DIRECTORY ${CMAKE_INSTALL_BINDIR}/apps/process-status.cpu-hog
    FILES
        apps/process-status.cpu-hog/info.yaml
        apps/process-status.cpu-hog/icon.png
        apps/process-status.cpu-hog/main.qml
)

qt_am_create_builtin_package(process-status.mem-hog
    OUTPUT_DIRECTORY apps/process-status.mem-hog
    INSTALL_DIRECTORY ${CMAKE_INSTALL_BINDIR}/apps/process-status.mem-hog
    FILES
        apps/process-status.mem-hog/info.yaml
        apps/process-status.mem-hog/icon.png
        apps/process-status.mem-hog/main.qml
)

qt_am_create_builtin_package(process-status.slim
    OUTPUT_DIRECTORY apps/process-status.slim
    INSTALL_DIRECTORY ${CMAKE_INSTALL_BINDIR}/apps/process-status.slim
    FILES
        apps/process-status.slim/info.yaml
        apps/process-status.slim/icon.png
        apps/process-status.slim/main.qml
)

install(TARGETS process-status
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
