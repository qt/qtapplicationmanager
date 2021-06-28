
qt_internal_add_cmake_library(app1plugin
    MODULE
    PUBLIC_LIBRARIES
        Qt::Core
        Qt::Gui
)

# Resources:
set(app1plugin_resource_files
    "qml/forms/AquaRect.qml"
)

qt_internal_add_resource(app1plugin "app1plugin"
    PREFIX
        "/app1"
    FILES
        ${app1plugin_resource_files}
)

qt_add_binary_resources(app1file app1file.qrc)

qt_autogen_tools_initial_setup(app1plugin)
