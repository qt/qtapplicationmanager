cmake_minimum_required(VERSION 3.16)
project(animated-windows LANGUAGES CXX)

find_package(Qt6 COMPONENTS AppManSystemUIPrivate)

qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

## System-UI ##

qt_add_executable(animated-windows WIN32
    main.cpp
)

if (QT_FEATURE_static)
    # This define flags us as an "appman" and allows us to link against the AppMan's private libraries
    target_compile_definitions(animated-windows PRIVATE AM_COMPILING_APPMAN)
endif()

qt_add_qml_module(animated-windows
    URI SystemUI
    QML_FILES
        system-ui/main.qml
    RESOURCES
        am-config.yaml
)

target_link_libraries(animated-windows PRIVATE
    Qt::AppManSystemUIPrivate
)

## Apps ##

qt_am_create_builtin_package(animated-windows.fish
    OUTPUT_DIRECTORY apps/animated-windows.fish
    INSTALL_DIRECTORY ${CMAKE_INSTALL_BINDIR}/apps/animated-windows.fish
    FILES
        apps/animated-windows.fish/info.yaml
        apps/animated-windows.fish/fish.svg
        apps/animated-windows.fish/fish.qml
)

qt_am_create_builtin_package(animated-windows.rabbit
    OUTPUT_DIRECTORY apps/animated-windows.rabbit
    INSTALL_DIRECTORY ${CMAKE_INSTALL_BINDIR}/apps/animated-windows.rabbit
    FILES
        apps/animated-windows.rabbit/info.yaml
        apps/animated-windows.rabbit/rabbit.svg
        apps/animated-windows.rabbit/rabbit.qml
)

install(TARGETS animated-windows
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
