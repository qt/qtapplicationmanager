
qt_am_internal_add_qml_test(tst_bubblewrap
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps netscript.sh good.env bad.env
    TEST_FILE tst_bubblewrap.qml
    CONFIGURATIONS
        CONFIG NAME multi-process CONDITION QT_FEATURE_am_multi_process ARGS --force-multi-process
)

add_subdirectory(apps/test.app)
