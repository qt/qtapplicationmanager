qt6_add_qml_module(
    sequelmoduleplugin
    VERSION 1.0
    URI "Sequel"
    PLUGIN_TARGET sequelmoduleplugin
    SOURCES terminator2.cpp
    QML_FILES CrashAnimation.qml
)

set(INSTALL_DIR "${INSTALL_EXAMPLEDIR}/apps/Crash/Sequel")

install(
    TARGETS sequelmoduleplugin
    LIBRARY DESTINATION "${INSTALL_DIR}"
    RUNTIME DESTINATION "${INSTALL_DIR}"
    BUNDLE DESTINATION "${INSTALL_DIR}"
)

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/qmldir DESTINATION "${INSTALL_DIR}")
