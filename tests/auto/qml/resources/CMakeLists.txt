
qt_internal_add_cmake_library(systemuiplugin
    MODULE
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    PUBLIC_LIBRARIES
        Qt::Core
)

# Resources:
set(systemuiplugin_resource_files
    "qml/widgets/MagentaRect.qml"
)

qt_internal_add_resource(systemuiplugin "systemuiplugin"
    PREFIX
        "/"
    FILES
        ${systemuiplugin_resource_files}
)

qt_am_internal_add_qml_test(tst_resource
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps relative qml
    TEST_FILE tst_resource.qml
)

qt_add_binary_resources(systemuifile systemuifile.qrc)

qt_autogen_tools_initial_setup(systemuiplugin)
add_subdirectory(appcommon)
add_subdirectory(apps/app1)
add_subdirectory(apps/app2)
