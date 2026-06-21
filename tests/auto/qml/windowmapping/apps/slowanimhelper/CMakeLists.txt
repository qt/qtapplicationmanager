set(CMAKE_AUTOMOC TRUE)

qt_add_qml_module(
    slowAnimHelperModule
    URI "SlowAnimHelper"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/SlowAnimHelper"
    # keep backing library and plugin as a single DLL: on Windows there is no
    # RPATH, so a separate plugin cannot find a backing lib in the parent dir
    PLUGIN_TARGET slowAnimHelperModule
    SOURCES slowanimhelper.h slowanimhelper.cpp
)

target_link_libraries(slowAnimHelperModule PRIVATE Qt::CorePrivate)
