qt_policy(SET QTP0001 NEW)   # "qt/qml/" default resource prefix

qt6_add_qml_module(
    systemuimodule
    VERSION 1.0
    URI "SystemUi"
    NO_PLUGIN
    RESOURCES grab.png close.png
    QML_FILES main.qml
)

set(INSTALL_DIR "${INSTALL_EXAMPLEDIR}/SystemUi")

install(
    TARGETS systemuimodule
    LIBRARY DESTINATION "${INSTALL_DIR}"
    RUNTIME DESTINATION "${INSTALL_DIR}"
    BUNDLE DESTINATION "${INSTALL_DIR}"
)
