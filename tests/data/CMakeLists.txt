
if (NOT QT_FEATURE_am_installer)
    message(WARNING "QT_FEATURE_am_installer is disabled, cannot generate test packages")
    return()
endif()

file(GLOB_RECURSE TEST_DATA_FILES
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    certificates/*
    hello-world.red/*
    manifests/*
    create-test-packages.sh
    icon.png
    info-big.yaml
    info.yaml
    testfile
    utilities.sh
)

add_custom_target(Test-Data-Files SOURCES
    ${TEST_DATA_FILES}
)

foreach(file ${TEST_DATA_FILES})
    if (NOT ${CMAKE_CURRENT_BINARY_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
        add_custom_command(OUTPUT ${file}
                           COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${file} ${CMAKE_CURRENT_BINARY_DIR}/${file}
                           DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${file}
                           COMMENT "Copying file: ${file}")
    endif()
    list(APPEND ALL_TEST_DATA_FILES ${CMAKE_CURRENT_BINARY_DIR}/${file})
endforeach()

qt_internal_collect_command_environment(env_path env_plugin_path)

qt_am_internal_find_host_packager()

if (WIN32)
    find_program(bash bash.exe
                 PATHS ENV PATH
                 HINTS "C:/Program Files/Git/bin"
                 HINTS "C:/Program Files (x86)/Git/bin"
    )

    if (bash STREQUAL "bash-NOTFOUND")
        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/packages
                           COMMAND "${CMAKE_COMMAND}" -E echo "Not generating test packages, because bash.exe could not be located."
        )
    else()
        # We need to set the PATH *inside* the bash, because MingW's bash.exe will always prepend
        # /mingw64/bin to $PATH, which in most cases has incompatible versions of the stdc++ and
        # gcc libs, preventing the appman-packager from starting.

        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/packages
                           COMMAND ${bash} -c 'export PATH="`cygpath -u -p \"${env_path}\"`:$$PATH" \; ${CMAKE_CURRENT_BINARY_DIR}/create-test-packages.sh $<TARGET_FILE:${QT_CMAKE_EXPORT_NAMESPACE}::appman-packager>'
                           DEPENDS ${ALL_TEST_DATA_FILES}
        )
    endif()
else()
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/packages
                       COMMAND "${CMAKE_COMMAND}" -E env "PATH=${env_path}${QT_PATH_SEPARATOR}$ENV{PATH}"
                               ${CMAKE_CURRENT_BINARY_DIR}/create-test-packages.sh $<TARGET_FILE:${QT_CMAKE_EXPORT_NAMESPACE}::appman-packager>
                       DEPENDS ${ALL_TEST_DATA_FILES}
    )
endif()

add_custom_target(create-test-packages ALL
                  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/packages
)
