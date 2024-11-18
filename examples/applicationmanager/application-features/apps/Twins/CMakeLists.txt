qt_add_qml_module(
    Twins_module
    URI "Twins"
    NO_PLUGIN
    QML_FILES twins.qml
)

if(WIN32)
    set_target_properties(Twins_module PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()

qt_am_create_builtin_package(Twins
    INSTALL_DIRECTORY "${INSTALL_EXAMPLEDIR}/apps/Twins"
    FILES
        info.yaml
        icon.png
    DEPENDENCIES
        Twins_module
)
