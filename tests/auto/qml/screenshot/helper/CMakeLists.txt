set(CMAKE_AUTOMOC TRUE)

qt_add_qml_module(
    screenshotHelperModule
    URI "ScreenshotHelper"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ScreenshotHelper"
    # keep backing library and plugin as a single DLL: on Windows there is no
    # RPATH, so a separate plugin cannot find a backing lib in the parent dir
    PLUGIN_TARGET screenshotHelperModule
    SOURCES screenshothelper.h screenshothelper.cpp
)

target_link_libraries(screenshotHelperModule PRIVATE Qt::Gui)
