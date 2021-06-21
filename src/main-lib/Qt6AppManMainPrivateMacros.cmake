function(qt_am_internal_create_copy_command file)
    if (NOT ${CMAKE_CURRENT_BINARY_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
        add_custom_command(OUTPUT ${file}
                        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${file} ${CMAKE_CURRENT_BINARY_DIR}/${file}
                        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${file}
                        COMMENT "Copying file: ${file}")
    endif()
    if (NOT ARG_NO_INSTALL)
        get_filename_component(dest ${INSTALL_EXAMPLEDIR}/${file} DIRECTORY)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${file}
            DESTINATION ${dest}
        )
    endif()
endfunction()

function(qt6_am_add_systemui_wrapper target)
    cmake_parse_arguments(
        PARSE_ARGV 1
        ARG
        "NO_INSTALL" "MAIN_QML_FILE" "CONFIG_YAML;EXTRA_FILES;EXTRA_ARGS"
    )

    file(CONFIGURE OUTPUT main.cpp
         CONTENT "int main(int, char *[]){}"
    )

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
        foreach(F ${ARG_EXTRA_FILES})
            file(GLOB_RECURSE MY_EXTRA_FILES
                RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
                "${F}/*"
            )
            list(APPEND ALL_EXTRA_FILES ${MY_EXTRA_FILES})
            foreach(B ${MY_EXTRA_FILES})
                qt_am_internal_create_copy_command(${B})
            endforeach()
        endforeach()
    endif()

    string(JOIN " " CMD_ARGS_STR ${CMD_ARGS})
    string(JOIN " " CMD_EXTRA_ARGS_STR ${ARG_EXTRA_ARGS})

    if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
        set(WRAPPER_SUFFIX ".bat")
        set(WRAPPER_SCRIPT "_${target}${WRAPPER_SUFFIX}")
        set(bindir "${QT_BUILD_INTERNALS_RELOCATABLE_INSTALL_PREFIX}/${INSTALL_BINDIR}")
        set(plugindir "${QT_BUILD_INTERNALS_RELOCATABLE_INSTALL_PREFIX}/${INSTALL_PLUGINSDIR}")
        get_filename_component(bindir "${bindir}" REALPATH)
        file(TO_NATIVE_PATH "${bindir}" bindir)
        get_filename_component(plugindir "${plugindir}" REALPATH)
        file(TO_NATIVE_PATH "${plugindir}" plugindir)

        file(GENERATE OUTPUT ${WRAPPER_SCRIPT} CONTENT
"@echo off
SetLocal EnableDelayedExpansion
(set PATH=${bindir};!PATH!)
if defined QT_PLUGIN_PATH (
    set QT_PLUGIN_PATH=${plugindir};!QT_PLUGIN_PATH!
) else (
    set QT_PLUGIN_PATH=${plugindir}
)
${CMAKE_INSTALL_PREFIX}/${INSTALL_BINDIR}/$<TARGET_FILE_NAME:${QT_CMAKE_EXPORT_NAMESPACE}::appman> ${CMD_ARGS_STR} ${CMD_EXTRA_ARGS_STR} ${ARG_MAIN_QML_FILE} %*
EndLocal
"
        )
    else()
        set(WRAPPER_SUFFIX ".sh")
        set(WRAPPER_SCRIPT "_${target}${WRAPPER_SUFFIX}")
        file(GENERATE OUTPUT ${WRAPPER_SCRIPT} CONTENT
"#!/bin/sh
exec ${CMAKE_INSTALL_PREFIX}/${INSTALL_BINDIR}/$<TARGET_FILE_NAME:${QT_CMAKE_EXPORT_NAMESPACE}::appman> ${CMD_ARGS_STR} ${CMD_EXTRA_ARGS_STR} ${ARG_MAIN_QML_FILE} \"$@\";
"
        )
    endif()


    add_executable(${target}
        ${CMAKE_CURRENT_BINARY_DIR}/main.cpp
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

    if (NOT ARG_NO_INSTALL)
        install(PROGRAMS $<TARGET_FILE:${target}>
            DESTINATION "${INSTALL_EXAMPLEDIR}"
        )
    endif()
endfunction()

if(NOT QT_NO_CREATE_VERSIONLESS_FUNCTIONS)
    function(qt_am_add_systemui_wrapper)
        qt6_am_add_systemui_wrapper(${ARGV})
    endfunction()
endif()
