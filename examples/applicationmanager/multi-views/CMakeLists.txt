cmake_minimum_required(VERSION 3.16)
project(multi-views LANGUAGES CXX)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/multi-views")


find_package(Qt6 COMPONENTS AppManMainPrivate)

qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

## System-UI ##

qt_add_executable(multi-views WIN32
    main.cpp
)

# This define flags us as an "appman" and allows us to link against the AppMan's private libraries
target_compile_definitions(multi-views PRIVATE AM_COMPILING_APPMAN)

qt_add_qml_module(multi-views
    URI SystemUI
    QML_FILES
        system-ui/main.qml
        system-ui/Readme.qml
    RESOURCES
        am-config.yaml
)

target_link_libraries(multi-views PRIVATE
    Qt::AppManMainPrivate
)

## Apps ##

qt6_am_create_builtin_package(tld.multi-views.app
    OUTPUT_DIRECTORY apps/tld.multi-views.app
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/apps/tld.multi-views.app
    FILES
        apps/tld.multi-views.app/info.yaml
        apps/tld.multi-views.app/icon.png
        apps/tld.multi-views.app/app1.qml
)

install(TARGETS multi-views
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)
