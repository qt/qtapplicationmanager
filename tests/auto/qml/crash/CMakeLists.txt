
qt_am_internal_add_qml_test(tst_crash
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps
    TEST_FILE tst_crash.qml
    CONFIGURATIONS
        CONFIG NAME multi-process CONDITION QT_FEATURE_am_multi_process ARGS --force-multi-process
)

add_subdirectory(apps/tld.test.crash)
