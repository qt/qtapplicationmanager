
qt_am_internal_add_qml_test(tst_intents
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps am-config-quick.yaml
    TEST_FILE tst_intents.qml
    CONFIGURATIONS
        CONFIG NAME single-process ARGS --force-single-process
        CONFIG NAME multi-process CONDITION QT_FEATURE_am_multi_process ARGS --force-multi-process
        CONFIG NAME config-quick CONDITION QT_FEATURE_am_multi_process ARGS --force-multi-process -c am-config-quick.yaml
)
