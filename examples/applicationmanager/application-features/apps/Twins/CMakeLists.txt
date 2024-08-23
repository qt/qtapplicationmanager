qt_add_qml_module(
    Twins_module
    URI "Twins"
    NO_PLUGIN
    QML_FILES twins.qml
)

qt_am_create_builtin_package(Twins
    INSTALL_DIRECTORY "${INSTALL_EXAMPLEDIR}/apps/Twins"
    FILES
        info.yaml
        icon.png
    DEPENDENCIES
        Twins_module
)
