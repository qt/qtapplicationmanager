set(CMAKE_AUTOMOC TRUE)

qt_add_qml_module(
    testAppModule
    URI "TestApp"
    NO_PLUGIN
    SOURCES helper.h helper.cpp
    QML_FILES app.qml
)
