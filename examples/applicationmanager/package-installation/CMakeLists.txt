
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

qt6_am_create_installable_package(hello-world-red
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ps-data/upload
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/ps-data/upload
    FILES
        installable-apps/hello-world.red/info.yaml
        installable-apps/hello-world.red/icon.png
        installable-apps/hello-world.red/main.qml
)

qt6_am_create_installable_package(hello-world-green
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ps-data/upload
    INSTALL_DIRECTORY ${INSTALL_EXAMPLEDIR}/ps-data/upload
    FILES
        installable-apps/hello-world.green/info.yaml
        installable-apps/hello-world.green/icon.png
        installable-apps/hello-world.green/main.qml
)

add_dependencies(package-installation hello-world-red hello-world-green)
