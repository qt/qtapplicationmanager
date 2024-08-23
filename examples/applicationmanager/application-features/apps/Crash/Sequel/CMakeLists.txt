qt_add_qml_module(
    sequelmoduleplugin
    URI "Sequel"
    PLUGIN_TARGET sequelmoduleplugin
    SOURCES terminator2.cpp
    QML_FILES CrashAnimation.qml
)

install(
    TARGETS sequelmoduleplugin
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}/apps/Crash/Sequel"
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}/apps/Crash/Sequel"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}/apps/Crash/Sequel"
)

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/qmldir DESTINATION "${INSTALL_EXAMPLEDIR}/apps/Crash/Sequel")
