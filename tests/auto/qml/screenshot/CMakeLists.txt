
qt_am_internal_add_qml_test(tst_screenshot
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps
    TEST_FILE tst_screenshot.qml
)

add_subdirectory(helper)
