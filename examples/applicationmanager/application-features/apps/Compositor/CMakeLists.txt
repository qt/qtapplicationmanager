qt6_add_qml_module(
    Compositor_module
    URI "Compositor"
    NO_PLUGIN
    QML_FILES compositor.qml
)

qt6_am_create_builtin_package(Compositor
    INSTALL_DIRECTORY "${INSTALL_EXAMPLEDIR}/apps/Compositor"
    FILES
        info.yaml
        icon.png
    DEPENDENCIES
        Compositor_module
)
