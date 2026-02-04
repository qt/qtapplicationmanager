set(CMAKE_AUTOMOC TRUE)

qt_add_qml_module(
    bwrapHelperModule
    URI "BwrapHelper"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/BwrapHelper"
    SOURCES helper.h helper.cpp
)
