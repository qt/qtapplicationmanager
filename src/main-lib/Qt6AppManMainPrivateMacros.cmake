set(_AM_MACROS_LOCATION ${CMAKE_CURRENT_LIST_DIR})

function(qt_am_internal_find_host_packager)
    if(TARGET ${QT_CMAKE_EXPORT_NAMESPACE}::appman-packager)
        return()
    endif()
    if(NOT QT_HOST_PATH)
        find_package(Qt6 COMPONENTS AppManMainPrivateTools)
        return()
    endif()
    # Try to find the host version of the packager:
    # Set up QT_HOST_PATH as an extra root path to look for the Tools package.
    # If toolchain file already provides host paths in a predefined order, we shouldn't break it.
    if(NOT "${QT_HOST_PATH_CMAKE_DIR}" IN_LIST CMAKE_PREFIX_PATH)
        list(PREPEND CMAKE_PREFIX_PATH "${QT_HOST_PATH_CMAKE_DIR}")
    endif()
    if(NOT "${QT_HOST_PATH}" IN_LIST CMAKE_FIND_ROOT_PATH)
        list(PREPEND CMAKE_FIND_ROOT_PATH "${QT_HOST_PATH}")
    endif()
    if(NOT "${QT_HOST_PATH}" IN_LIST CMAKE_PROGRAM_PATH)
        list(PREPEND CMAKE_PROGRAM_PATH "${QT_HOST_PATH}")
    endif()
    # This can't use the find_package(Qt6 COMPONENTS) signature, because Qt6Config uses NO_DEFAULT
    # and won't look at the prepend extra find root paths.
    find_package(Qt6AppManMainPrivateTools ${PROJECT_VERSION} CONFIG
        PATHS
            ${_qt_additional_packages_prefix_path}
            ${_qt_additional_packages_prefix_path_env}
            ${QT_HOST_PATH_CMAKE_DIR}
        NO_DEFAULT_PATH
    )
endfunction()

function(qt_am_internal_create_copy_command file)
    if (NOT ${CMAKE_CURRENT_BINARY_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
        add_custom_command(OUTPUT ${file}
                        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${file} ${CMAKE_CURRENT_BINARY_DIR}/${file}
                        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${file}
                        COMMENT "Copying file: ${file}")
    endif()
    if (ARG_INSTALL_DIR)
        get_filename_component(dest ${ARG_INSTALL_DIR}/${file} DIRECTORY)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${file}
            DESTINATION ${dest}
        )
    endif()
endfunction()

function(qt6_am_add_systemui_wrapper target)
    cmake_parse_arguments(
        PARSE_ARGV 1
        ARG
        "" "MAIN_QML_FILE;EXECUTABLE;INSTALL_DIR" "CONFIG_YAML;EXTRA_FILES;EXTRA_FILES_GLOB;EXTRA_ARGS"
    )

    if (NOT ARG_EXECUTABLE)
        set(ARG_EXECUTABLE "appman")
    endif()

    set(CMD_ARGS)
    set(ALL_EXTRA_FILES)
    if (ARG_MAIN_QML_FILE)
        list(APPEND ALL_EXTRA_FILES ${ARG_MAIN_QML_FILE})
        qt_am_internal_create_copy_command(${ARG_MAIN_QML_FILE})
    endif()

    if (ARG_CONFIG_YAML)
        foreach(F ${ARG_CONFIG_YAML})
            list(APPEND CMD_ARGS "-c ${F}")
            list(APPEND ALL_EXTRA_FILES ${F})
            qt_am_internal_create_copy_command(${F})
        endforeach()
    endif()

    if (ARG_EXTRA_FILES)
        set(GLOB_BASE_PATTERN *.qml *.js qmldir *.json *.yaml *.png *.jpg *.svg)
        if (ARG_EXTRA_FILES_GLOB)
            set(GLOB_BASE_PATTERN ${ARG_EXTRA_FILES_GLOB})
        endif()

        foreach(F ${ARG_EXTRA_FILES})
            if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${F})
                set(GLOB_PATTERN ${GLOB_BASE_PATTERN})
                list(TRANSFORM GLOB_PATTERN PREPEND "${F}/")
                file(GLOB_RECURSE MY_EXTRA_FILES
                    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
                    ${GLOB_PATTERN}
                )
            else()
                set(MY_EXTRA_FILES ${F})
            endif()
            list(APPEND ALL_EXTRA_FILES ${MY_EXTRA_FILES})
            foreach(B ${MY_EXTRA_FILES})
                qt_am_internal_create_copy_command(${B})
            endforeach()
        endforeach()
    endif()

    string(JOIN " " CMD_ARGS_STR ${CMD_ARGS})
    string(JOIN " " CMD_EXTRA_ARGS_STR ${ARG_EXTRA_ARGS})

    configure_file(${_AM_MACROS_LOCATION}/wrapper.cpp.in wrapper.cpp)

    if (COMMAND qt_internal_collect_command_environment)
        qt_internal_collect_command_environment(test_env_path test_env_plugin_path)
    else()
        set(test_env_path "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
        set(test_env_plugin_path "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}")
    endif()

    if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
        set(WRAPPER_SUFFIX ".bat")
        set(WRAPPER_SCRIPT "_${target}$<CONFIG>${WRAPPER_SUFFIX}")

        file(GENERATE OUTPUT ${WRAPPER_SCRIPT} CONTENT
"@echo off
cd \"%~dp0\"
SetLocal EnableDelayedExpansion
(set \"PATH=${test_env_path};%PATH%\")
(set \"QT_PLUGIN_PATH=${test_env_plugin_path}\")
${ARG_EXECUTABLE}.exe ${CMD_ARGS_STR} ${CMD_EXTRA_ARGS_STR} ${ARG_MAIN_QML_FILE} %*
EndLocal
"
        )
    else()
        set(WRAPPER_SUFFIX ".sh")
        set(WRAPPER_SCRIPT "_${target}$<CONFIG>${WRAPPER_SUFFIX}")
        file(GENERATE OUTPUT ${WRAPPER_SCRIPT} CONTENT
"#!/bin/sh
cd \"\${0%/*}\"
export PATH=\"${test_env_path}:$PATH\"
export QT_PLUGIN_PATH=\"${test_env_plugin_path}\"
exec ${ARG_EXECUTABLE} ${CMD_ARGS_STR} ${CMD_EXTRA_ARGS_STR} ${ARG_MAIN_QML_FILE} \"$@\";
"
        )
    endif()


    add_executable(${target}
        ${CMAKE_CURRENT_BINARY_DIR}/wrapper.cpp
        # Add all files we copy to the build folder as sources to the main executable
        # This makes all files show up in the IDE
        ${ALL_EXTRA_FILES}
    )

    add_custom_target(${target}_deploy
        DEPENDS ${ALL_EXTRA_FILES}
    )

    set_target_properties(${target} PROPERTIES SUFFIX "${WRAPPER_SUFFIX}")

    add_dependencies(${target} ${target}_deploy)

    set(EXTRA_COMMAND "")
    if ("${WRAPPER_SUFFIX}" STREQUAL ".sh")
        set(EXTRA_COMMAND COMMAND chmod +x ${WRAPPER_SCRIPT})
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        ${EXTRA_COMMAND}
        COMMAND ${CMAKE_COMMAND} -E copy ${WRAPPER_SCRIPT} $<TARGET_FILE_NAME:${target}>
    )

    if (ARG_INSTALL_DIR)
        install(PROGRAMS $<TARGET_FILE:${target}>
            DESTINATION "${ARG_INSTALL_DIR}"
        )
    endif()
endfunction()

if(NOT QT_NO_CREATE_VERSIONLESS_FUNCTIONS)
    function(qt_am_add_systemui_wrapper)
        qt6_am_add_systemui_wrapper(${ARGV})
    endfunction()
endif()

function (qt_am_internal_add_qml_test target)
    cmake_parse_arguments(
        PARSE_ARGV 1
        ARG
        "" "TEST_FILE;TESTDATA_DIR" "CONFIG_YAML;EXTRA_FILES;CONFIGURATIONS"
    )

    if (NOT ARG_TEST_FILE)
        message(FATAL_ERROR "TEST_FILE needs to be provided")
    endif()

    if (NOT ARG_CONFIGURATIONS)
        set(ARG_CONFIGURATIONS CONFIG NAME single-process ARGS --force-single-process)

        if (QT_FEATURE_am_multi_process)
            list(APPEND ARG_CONFIGURATIONS CONFIG NAME multi-process ARGS --force-multi-process)
        endif()
    endif()

    # Parse the CONFIGURATIONS arguments and split it into multiple configurations
    # Each configuration needs to start with the CONFIG keyword
    # Because cmake doesn't support nested lists, we create a string for every configuration
    set(CONFIGS)
    foreach(CONFIGURATION ${ARG_CONFIGURATIONS})
        if (CONFIGURATION STREQUAL "CONFIG")
                list(JOIN CONFIG " " CONFIG_STR)
                list(APPEND CONFIGS "${CONFIG_STR}")
                set(CONFIG)
        else()
            list(APPEND CONFIG ${CONFIGURATION})
        endif()
    endforeach()
    list(JOIN CONFIG " " CONFIG_STR)
    list(APPEND CONFIGS "${CONFIG_STR}")

    foreach(CONFIG ${CONFIGS})
        # Convert the configuration string back to a list of options
        string(REPLACE " " ";" CONFIG ${CONFIG})
        cmake_parse_arguments(
            CONFIG_ARG
            "" "NAME;CONDITION" "ARGS" ${CONFIG}
        )

        if (NOT CONFIG_ARG_NAME)
            message(FATAL_ERROR "CONFIGURATION needs to have a NAME")
        endif()

        if (NOT CONFIG_ARG_CONDITION)
            set(CONFIG_ARG_CONDITION ON)
        endif()

        qt_evaluate_config_expression(result ${CONFIG_ARG_CONDITION})
        if (${result})
            set(WRAPPER_ARGS)
            if (ARG_CONFIG_YAML)
                list(APPEND WRAPPER_ARGS CONFIG_YAML ${ARG_CONFIG_YAML})
            endif()

            if (ARG_EXTRA_FILES)
                list(APPEND WRAPPER_ARGS EXTRA_FILES ${ARG_EXTRA_FILES})
            endif()

            list(APPEND WRAPPER_ARGS EXTRA_ARGS
                --qmltestrunner-source-file "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_TEST_FILE}")

            if (ARG_TESTDATA_DIR)
                list(APPEND WRAPPER_ARGS EXTRA_ARGS -o "\"systemProperties: { public: { AM_TESTDATA_DIR: ${ARG_TESTDATA_DIR} } }\"")
            endif()

            list(APPEND WRAPPER_ARGS EXTRA_ARGS --no-cache --no-dlt-logging)
            if (APPLE)
                list(APPEND WRAPPER_ARGS EXTRA_ARGS --dbus=none)
            endif()

            if (CONFIG_ARG_ARGS)
                list(APPEND WRAPPER_ARGS EXTRA_ARGS ${CONFIG_ARG_ARGS} --)
            else()
                list(APPEND WRAPPER_ARGS EXTRA_ARGS --)
            endif()

            qt6_am_add_systemui_wrapper(${target}_${CONFIG_ARG_NAME}
                EXECUTABLE appman-qmltestrunner
                MAIN_QML_FILE ${ARG_TEST_FILE}
                ${EXTRA_FILES_ARG_STR}
                ${WRAPPER_ARGS}
            )

            qt_internal_add_test(${target}_${CONFIG_ARG_NAME}
            )
        endif()
    endforeach()

endfunction()

