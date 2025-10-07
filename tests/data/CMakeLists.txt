
qt_feature_evaluate_features("${CMAKE_CURRENT_SOURCE_DIR}/../../src/common-lib/configure.cmake")

if (NOT QT_FEATURE_am_installer)
    message(WARNING "QT_FEATURE_am_installer is disabled, cannot generate test packages")
    return()
endif()

if (DEFINED ENV{QT_AM_DISABLE_PACKAGE_GENERATION}) # for Coin
    message(WARNING "Test package generation is disabled via $QT_AM_DISABLE_PACKAGE_GENERATION")
    return()
endif()
if (QNX OR ANDROID OR IOS)
    message(WARNING "Test package generation is not supported on QNX, Android and iOS")
    return()
endif()

set(TEST_DATA_FILES
    binaries/README.md
    binaries/android-elf.so.zz
    binaries/macos-macho.zz
    binaries/macos-universal.zz
    binaries/windows-pe.exe.zz
    certificates/create-test-certificates.sh
    certificates/openssl-ca.cnf
    certificates/openssl-dev1.cnf
    certificates/openssl-dev2.cnf
    certificates/openssl-devca.cnf
    certificates/openssl-other-ca.cnf
    certificates/openssl-store.cnf
    create-test-packages.sh
    icon.png
    info.yaml
    info-other.yaml
    testfile
    utilities.sh
)

add_custom_target(Test-Data-Files SOURCES
    ${TEST_DATA_FILES}
)

foreach(file ${TEST_DATA_FILES})
    if (NOT ${CMAKE_CURRENT_BINARY_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${file}
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
        set(appman_packager_found FALSE)
        set(appman_packager_target "${QT_CMAKE_EXPORT_NAMESPACE}::appman-packager")
        if(CMAKE_HOST_WIN32 AND CMAKE_CROSSCOMPILING)
            if (TARGET "${appman_packager_target}")
                set(appman_packager_found TRUE)
                get_target_property(packager ${appman_packager_target} IMPORTED_LOCATION)
                cmake_path(GET packager ROOT_NAME root_name)
            else()
                set(path "${QT_HOST_PATH}/${QT6_HOST_INFO_BINDIR}/")
                cmake_path(GET path ROOT_NAME root_name)
            endif()
            if(NOT root_name)
                set(disk "C:")
            else()
                set(disk "")
            endif()
            if(appman_packager_found)
                set(packager "${disk}${packager}")
            else()
                set(packager "${disk}${path}$<TARGET_FILE_NAME:${appman_packager_target}>")
            endif()
        else()
            set(packager "$<TARGET_FILE:${appman_packager_target}>")
        endif()
        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/packages
                           COMMAND ${bash} -c 'export PATH="`cygpath -u -p \"${env_path}\"`:$$PATH" \; ${CMAKE_CURRENT_BINARY_DIR}/create-test-packages.sh \"${packager}\"'
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
