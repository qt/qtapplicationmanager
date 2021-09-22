
cmake_minimum_required(VERSION 3.16)
project(application-features LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/application-features")

find_package(Qt6 COMPONENTS AppManMainPrivate)

qt6_am_add_systemui_wrapper(application-features
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps system-ui
)

add_subdirectory(imports)
add_subdirectory(native)
