cmake_minimum_required(VERSION 3.16)
project(application-features LANGUAGES CXX)

find_package(Qt6 COMPONENTS AppManSystemUIPrivate)

qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

## System-UI ##

qt_add_executable(application-features
    main.cpp
)

if (QT_FEATURE_static)
    # This define flags us as an "appman" and allows us to link against the AppMan's private libraries
    target_compile_definitions(application-features PRIVATE AM_COMPILING_APPMAN)
endif()

qt_add_qml_module(application-features
    URI "SystemUI"
    QML_FILES
        SystemUi/main.qml
    RESOURCES
        SystemUi/grab.png
        SystemUi/close.png
        am-config.yaml
)

target_link_libraries(application-features PRIVATE
    Qt::AppManSystemUIPrivate
)

## Apps ##

add_subdirectory(apps/Twins)
add_subdirectory(apps/Compositor)
add_subdirectory(apps/Crash)
add_subdirectory(apps/Glitches)
if(QT_FEATURE_am_multi_process)
    add_subdirectory(apps/Widgets)
endif()

install(TARGETS application-features
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
