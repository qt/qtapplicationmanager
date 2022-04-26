
qt_am_internal_add_qml_test(tst_processtitle
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps am-config-quick.yaml
    TEST_FILE tst_processtitle.qml
    CONFIGURATIONS
        CONFIG NAME multi-process CONDITION QT_FEATURE_am_multi_process ARGS --force-multi-process
        CONFIG NAME config-quick CONDITION QT_FEATURE_am_multi_process ARGS --force-multi-process -c am-config-quick.yaml
)
