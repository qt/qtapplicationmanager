
qt_internal_add_cmake_library(app1plugin
    MODULE
    LIBRARIES
        Qt::Core
        Qt::Gui
)

qt_internal_add_resource(app1plugin "app1plugin"
    PREFIX
        "/app1"
    FILES
        "qml/forms/AquaRect.qml"
)

qt_add_binary_resources(app1file app1file.qrc)

qt_autogen_tools_initial_setup(app1plugin)
