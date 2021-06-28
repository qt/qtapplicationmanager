
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

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/packages
                   COMMAND ${CMAKE_CURRENT_BINARY_DIR}/create-test-packages.sh $<TARGET_FILE:${QT_CMAKE_EXPORT_NAMESPACE}::appman-packager>
                   DEPENDS ${ALL_TEST_DATA_FILES}
                   )

add_custom_target(create-test-packages ALL
                  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/packages
                  )
