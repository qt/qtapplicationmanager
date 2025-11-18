qt_add_qml_module(
    Glitches_module
    URI "Glitches"
    NO_PLUGIN
    SOURCES glitches.cpp glitches.h
    QML_FILES glitchesapp.qml
)

target_link_libraries(Glitches_module PRIVATE Qt::Quick)

if(WIN32)
    set_target_properties(Glitches_module PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()

qt_am_create_builtin_package(Glitches
    INSTALL_DIRECTORY "${CMAKE_INSTALL_BINDIR}/apps/Glitches"
    FILES
        info.yaml
        icon.png
    DEPENDENCIES
        Glitches_module
)
