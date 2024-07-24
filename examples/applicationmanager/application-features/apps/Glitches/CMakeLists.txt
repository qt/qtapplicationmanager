qt6_add_qml_module(
    Glitches_module
    URI "Glitches"
    NO_PLUGIN
    QML_FILES glitchesapp.qml
)

qt6_am_create_builtin_package(Glitches
    INSTALL_DIRECTORY "${INSTALL_EXAMPLEDIR}/apps/Glitches"
    FILES
        info.yaml
        icon.png
    DEPENDENCIES
        Glitches_module
)

add_subdirectory(Glitches)
