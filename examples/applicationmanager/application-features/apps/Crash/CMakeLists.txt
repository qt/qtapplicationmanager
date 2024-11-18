
qt_add_qml_module(
    Crash_module
    URI "Crash"
    NO_PLUGIN
    SOURCES terminator1.cpp
    QML_FILES crashapp.qml
)

if (WIN32)
  set_target_properties(Crash_module PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endif()

qt_am_create_builtin_package(Crash
    INSTALL_DIRECTORY "${INSTALL_EXAMPLEDIR}/apps/Crash"
    FILES
        info.yaml
        icon.png
    DEPENDENCIES
        Crash_module
)

add_subdirectory(Sequel)
