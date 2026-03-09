cmake_minimum_required(VERSION 3.16)
project(package-installation LANGUAGES CXX)

find_package(Qt6 COMPONENTS AppManSystemUIPrivate)

qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

## System-UI ##

qt_add_executable(package-installation
    main.cpp
)

if (QT_FEATURE_static)
    # This define flags us as an "appman" and allows us to link against the AppMan's private libraries
    target_compile_definitions(package-installation PRIVATE AM_COMPILING_APPMAN)
endif()

if(CMAKE_CROSSCOMPILING)
    set(SYSPROP_CROSS   "crossBuild: true")
    set(SYSPROP_BINARY  "")
    set(SYSPROP_BASEDIR "")
else()
    set(SYSPROP_CROSS   "")
    set(SYSPROP_BINARY  "serverBinary: '${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/appman-package-server'")
    set(SYSPROP_BASEDIR "serverBaseDir: '\${EXECUTABLE_DIR}/ps-data'")
endif()

file(CONFIGURE @ONLY OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/extra.yaml CONTENT
"
formatVersion: 1
formatType: am-configuration
---
systemProperties:
  public:
    @SYSPROP_CROSS@
    @SYSPROP_BINARY@
    @SYSPROP_BASEDIR@
"
)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/extra.yaml PROPERTIES QT_RESOURCE_ALIAS extra.yaml)

qt_add_qml_module(package-installation
    URI SystemUI
    QML_FILES
        system-ui/main.qml
        system-ui/AcknowledgeDialog.qml
        system-ui/PackageServerInterface.qml
    RESOURCES
        system-ui/install.svg
        system-ui/package.svg
        system-ui/reload.svg
        system-ui/uninstall.svg
        am-config.yaml
        ${CMAKE_CURRENT_BINARY_DIR}/extra.yaml
)

target_link_libraries(package-installation PRIVATE
    Qt::AppManSystemUIPrivate
)

## Apps ##

qt_am_create_builtin_package(builtin.blue
    OUTPUT_DIRECTORY apps/builtin.blue
    INSTALL_DIRECTORY ${CMAKE_INSTALL_BINDIR}/apps/builtin.blue
    FILES
        apps/builtin.blue/info.yaml
        apps/builtin.blue/icon.png
        apps/builtin.blue/main.qml
)

qt_am_create_installable_package(installable-red
    OUTPUT_DIRECTORY ps-data/upload
    INSTALL_DIRECTORY ${CMAKE_INSTALL_BINDIR}/ps-data/upload
    FILES
        installable-apps/installable.red/info.yaml
        installable-apps/installable.red/icon.png
        installable-apps/installable.red/main.qml
)

qt_am_create_installable_package(installable-green
    OUTPUT_DIRECTORY ps-data/upload
    INSTALL_DIRECTORY ${CMAKE_INSTALL_BINDIR}/ps-data/upload
    FILES
        installable-apps/installable.green/info.yaml
        installable-apps/installable.green/icon.png
        installable-apps/installable.green/main.qml
)

set_source_files_properties(
    installable-apps/installable.pink/main.qml
    PROPERTIES
    QT_RESOURCE_ALIAS main.qml
)
qt_add_qml_module(installable-pink-module
    URI "Pink"
    NO_PLUGIN
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Pink"
    QML_FILES installable-apps/installable.pink/main.qml
)
set_target_properties(installable-pink-module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/installable-pink_package
)

qt_am_create_installable_package(installable-pink
    OUTPUT_DIRECTORY ps-data/upload
    INSTALL_DIRECTORY ${CMAKE_INSTALL_BINDIR}/ps-data/upload
    FILES
        installable-apps/installable.pink/info.yaml
        installable-apps/installable.pink/icon.png
    DEPENDENCIES
        installable-pink-module
)

add_dependencies(package-installation installable-red installable-green installable-pink)

install(TARGETS package-installation
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
