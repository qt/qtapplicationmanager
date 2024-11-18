qt_add_qml_module(
    Compositor_module
    URI "Compositor"
    NO_PLUGIN
    QML_FILES main.qml Compositor.qml
)

if(WIN32)
    set_target_properties(Compositor_module PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()

qt_am_create_builtin_package(Compositor
    INSTALL_DIRECTORY "${INSTALL_EXAMPLEDIR}/apps/Compositor"
    FILES
        info.yaml
        icon.png
    DEPENDENCIES
        Compositor_module
)
