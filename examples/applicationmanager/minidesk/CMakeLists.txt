cmake_minimum_required(VERSION 3.16)
project(minidesk LANGUAGES CXX)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/minidesk")


find_package(Qt6 COMPONENTS AppManMainPrivate)

qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

## System-UI ##

qt_add_executable(minidesk WIN32
    main.cpp
)

# This define flags us as an "appman" and allows us to link against the AppMan's private libraries
target_compile_definitions(minidesk PRIVATE AM_COMPILING_APPMAN)

qt_add_qml_module(minidesk
    URI SystemUI
    QML_FILES
        system-ui/main.qml
        system-ui/Readme.qml
    RESOURCES
        system-ui/chrome-active.png
        system-ui/chrome-bg.png
        system-ui/close.png
        am-config.yaml
)

target_link_libraries(minidesk PRIVATE
    Qt::AppManMainPrivate
)

## Apps ##


qt_am_create_builtin_package(tld.minidesk.app1
    OUTPUT_DIRECTORY apps/tld.minidesk.app1
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/tld.minidesk.app1
    FILES
        apps/tld.minidesk.app1/info.yaml
        apps/tld.minidesk.app1/icon.png
        apps/tld.minidesk.app1/app1.qml
)

qt_am_create_builtin_package(tld.minidesk.app2
    OUTPUT_DIRECTORY apps/tld.minidesk.app2
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/tld.minidesk.app2
    FILES
        apps/tld.minidesk.app2/info.yaml
        apps/tld.minidesk.app2/icon.png
        apps/tld.minidesk.app2/app2.qml
)

install(TARGETS minidesk
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)
