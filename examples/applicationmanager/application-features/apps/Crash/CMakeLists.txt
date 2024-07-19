
qt6_add_qml_module(
    Crash_module
    URI "Crash"
    NO_PLUGIN
    SOURCES terminator1.cpp
    QML_FILES crashapp.qml
)

qt6_am_create_builtin_package(Crash
    INSTALL_DIRECTORY "${INSTALL_EXAMPLEDIR}/apps/Crash"
    FILES
        info.yaml
        icon.png
    DEPENDENCIES
        Crash_module
)

add_subdirectory(Sequel)
