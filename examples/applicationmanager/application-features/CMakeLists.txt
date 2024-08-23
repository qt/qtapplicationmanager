cmake_minimum_required(VERSION 3.16)
project(application-features LANGUAGES CXX)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
    set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/application-features")

find_package(Qt6 COMPONENTS AppManMainPrivate)

qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

## System-UI ##

qt_add_executable(application-features WIN32
    main.cpp
)

# This define flags us as an "appman" and allows us to link against the AppMan's private libraries
target_compile_definitions(application-features PRIVATE AM_COMPILING_APPMAN)

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
    Qt::AppManMainPrivate
)

## Apps ##

add_subdirectory(apps/Twins)
add_subdirectory(apps/Compositor)
add_subdirectory(apps/Crash)
add_subdirectory(apps/Glitches)
add_subdirectory(apps/Widgets)

install(TARGETS application-features
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)
