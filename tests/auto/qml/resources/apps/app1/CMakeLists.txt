# Generated from app1.pro.

#####################################################################
## app1plugin Generic Library:
#####################################################################

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


#### Keys ignored in scope 1:.:.:app1.pro:<TRUE>:
# OTHER_FILES = "info.yaml" "icon.png"
# RESOURCE_SOURCE = "app1file.qrc"
# TEMPLATE = "lib"

qt_autogen_tools_initial_setup(app1plugin)
