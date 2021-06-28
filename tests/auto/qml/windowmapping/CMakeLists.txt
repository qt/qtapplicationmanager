
qt_am_internal_add_qml_test(tst_windowmapping
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps
    TEST_FILE tst_windowmapping.qml
    CONFIGURATIONS #TODO: temporary - multi-process crashes in RHI on exit
        CONFIG NAME single-process ARGS --force-single-process
)
