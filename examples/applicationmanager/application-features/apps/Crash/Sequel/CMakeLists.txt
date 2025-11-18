qt_add_qml_module(
    sequelmoduleplugin
    URI "Sequel"
    PLUGIN_TARGET sequelmoduleplugin
    SOURCES terminator2.cpp
    QML_FILES CrashAnimation.qml
)

install(
    TARGETS sequelmoduleplugin
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}/apps/Crash/Sequel"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/apps/Crash/Sequel"
)

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/qmldir DESTINATION "${CMAKE_INSTALL_BINDIR}/apps/Crash/Sequel")
