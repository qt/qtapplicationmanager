
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
)

#TODO: check if it makes sense to expose this as public cmake API
function(qt6_am_create_package target)
    cmake_parse_arguments(
        PARSE_ARGV 1
        ARG
        "" "SOURCE_DIR;OUTPUT_PACKAGE" ""
    )

    if (NOT ARG_SOURCE_DIR)
        message(FATAL_ERROR "SOURCE_DIR needs to be provided")
    endif()
    if (NOT EXISTS "${ARG_SOURCE_DIR}/info.yaml")
        message(FATAL_ERROR "SOURCE_DIR does not contain info.yaml")
    endif()
    if (NOT ARG_OUTPUT_PACKAGE)
        message(FATAL_ERROR "OUTPUT_PACKAGE needs to be provided")
    endif()

    if (COMMAND qt_internal_collect_command_environment)
        qt_internal_collect_command_environment(env_path env_plugin_path)
    else()
        set(env_path "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
        set(env_plugin_path "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}")
    endif()

    qt_am_internal_find_host_packager()

    add_custom_command(
        OUTPUT  ${ARG_OUTPUT_PACKAGE}
        COMMAND ${CMAKE_COMMAND} -E env "PATH=${env_path}${QT_PATH_SEPARATOR}$ENV{PATH}"
                    $<TARGET_FILE:${QT_CMAKE_EXPORT_NAMESPACE}::appman-packager>
                    create-package ${ARG_OUTPUT_PACKAGE} ${ARG_SOURCE_DIR}
        DEPENDS ${ARG_SOURCE_DIR}
        VERBATIM
    )
    add_custom_target(${target} DEPENDS ${ARG_OUTPUT_PACKAGE})
endfunction()


foreach (PKG_NAME red green)
    qt6_am_create_package(package-installation-create-${PKG_NAME}-ampkg
        SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/installable-apps/hello-world.${PKG_NAME}/
        OUTPUT_PACKAGE ${CMAKE_CURRENT_BINARY_DIR}/ps-data/upload/hello-${PKG_NAME}.ampkg
    )
    add_dependencies(package-installation package-installation-create-${PKG_NAME}-ampkg)
endforeach()
