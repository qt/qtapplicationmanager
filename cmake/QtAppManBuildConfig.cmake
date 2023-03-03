# This function adds the current build configuration to the specified target
# as a YAML file named ":/build-config.yaml" via a Qt resource file.
function(qtam_internal_add_build_config target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Trying to extend non-existing target \"${target}\".")
    endif()

    # get the git version, if available
    if(EXISTS "${PROJECT_SOURCE_DIR}/.tag")
        file(READ ${PROJECT_SOURCE_DIR}/.tag GIT_VERSION)
        STRING(REGEX REPLACE "\n" "" GIT_VERSION "${GIT_VERSION}")
        if(GIT_VERSION STREQUAL "\$Format:%H\$")
            set(GIT_VERSION "unknown")
            if(EXISTS ${CMAKE_SOURCE_DIR}/.git)
                execute_process(
                    COMMAND git describe --tags --always --dirty
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    OUTPUT_VARIABLE GIT_VERSION
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
                )
            endif()
        endif()
    else()
        set(GIT_VERSION "")
    endif()

    # generate the necessary strings to be backward compatible with qmake
    string(TIMESTAMP _DATE_ UTC)
    set(SOURCE_DIR ${CMAKE_SOURCE_DIR})
    set(BUILD_DIR ${CMAKE_BINARY_DIR})
    set(MODULE_VERSION ${PROJECT_VERSION})
    set(INSTALL_PREFIX ${QT6_INSTALL_PREFIX})
    set(QT_ARCH ${CMAKE_SYSTEM_PROCESSOR})
    set(QT_VERSION ${Qt6_VERSION})
    get_target_property(DEFINES ${target} COMPILE_DEFINITIONS)
    set(DEFINES_TYPE "array")

    get_cmake_property(ALL_VARS VARIABLES)
    foreach (VAR ${ALL_VARS})
        if (VAR MATCHES "^QT_FEATURE_([a-z])")
            list(APPEND QT_FEATURES "${VAR}")
        endif()
    endforeach()
    set(QT_FEATURES_TYPE "dict")

    set(build_config "${CMAKE_CURRENT_BINARY_DIR}/build-config.yaml")
    file(WRITE "${build_config}" "---\n")
    foreach(VAR _DATE_ MODULE_VERSION GIT_VERSION SOURCE_DIR BUILD_DIR INSTALL_PREFIX
                QT_ARCH QT_VERSION QT_FEATURES DEFINES)
        if(NOT VAR)
            file(APPEND "${build_config}" "${VAR}: ~\n")
        elseif("${${VAR}_TYPE}" STREQUAL "array")
            file(APPEND "${build_config}" "${VAR}:\n")
            foreach(VAL ${${VAR}})
               file(APPEND "${build_config}" "  - '${VAL}'\n")
            endforeach()
        elseif("${${VAR}_TYPE}" STREQUAL "dict")
            file(APPEND "${build_config}" "${VAR}:\n")
            foreach(SUBVAR ${${VAR}})
               set(VAL ${${SUBVAR}})
               if(VAL STREQUAL "ON")
                   set(VAL "yes")
               elseif(VAL STREQUAL "OFF")
                   set(VAL "no")
               else()
                   set(VAL "'${VAL}'")
               endif()
               file(APPEND "${build_config}" "  ${SUBVAR}: ${VAL}\n")
            endforeach()
        else()
            file(APPEND "${build_config}" "${VAR}: '${${VAR}}'\n")
        endif()
    endforeach()

    qt_internal_add_resource(${target} "build-config"
        PREFIX "/"
        FILES "${build_config}"
        BASE "${CMAKE_CURRENT_BINARY_DIR}"
    )
endfunction()
