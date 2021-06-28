
qt_am_internal_add_qml_test(tst_installer
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps
    TESTDATA_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../../data
    TEST_FILE tst_installer.qml
)
