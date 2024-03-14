
qt_am_internal_add_qml_test(tst_windowproperties
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps
    TEST_FILE tst_windowproperties.qml
    CONFIGURATIONS
        CONFIG NAME single-process ARGS --force-single-process
        CONFIG NAME multi-process CONDITION QT_FEATURE_am_multi_process ARGS --force-multi-process
)

if (QT_FEATURE_am_multi_process)
    add_subdirectory(apps/wp-app-native)
endif()
