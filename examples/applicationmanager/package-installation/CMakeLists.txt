
cmake_minimum_required(VERSION 3.16)
project(package-installation LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/package-installation")

find_package(Qt6 COMPONENTS Core)
find_package(Qt6 COMPONENTS Gui)
find_package(Qt6 COMPONENTS AppManMainPrivate)
find_package(Qt6 COMPONENTS AppManMainPrivateTools)

qt6_am_add_systemui_wrapper(package-installation
    INSTALL_DIR "${INSTALL_EXAMPLEDIR}"
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps system-ui
    EXTRA_ARGS --verbose
               -o "systemProperties: { public: { serverBinary: '${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/appman-package-server' }}"
               -o "systemProperties: { public: { serverBaseDir: '${CMAKE_CURRENT_BINARY_DIR}/ps-data' }}"
)

foreach (PKG_NAME red green)
    qt6_am_create_installable_package(package-installation-create-${PKG_NAME}-ampkg
        SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/installable-apps/hello-world.${PKG_NAME}/
        OUTPUT_PACKAGE ${CMAKE_CURRENT_BINARY_DIR}/ps-data/upload/hello-${PKG_NAME}.ampkg
    )
    add_dependencies(package-installation package-installation-create-${PKG_NAME}-ampkg)
endforeach()
