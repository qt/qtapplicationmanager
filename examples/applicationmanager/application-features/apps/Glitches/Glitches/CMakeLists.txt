qt_add_qml_module(
    glitchesmoduleplugin
    URI "Glitches"
    PLUGIN_TARGET glitchesmoduleplugin
    SOURCES glitches.cpp
)

target_link_libraries(glitchesmoduleplugin PRIVATE Qt::Quick)

install(
    TARGETS glitchesmoduleplugin
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}/apps/Glitches/Glitches"
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}/apps/Glitches/Glitches"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}/apps/Glitches/Glitches"
)

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/qmldir DESTINATION "${INSTALL_EXAMPLEDIR}/apps/Glitches/Glitches")
