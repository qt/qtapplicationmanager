
qt_am_internal_add_qml_test(tst_configs
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps am-config-nodbus.yaml
    TEST_FILE tst_configs.qml
    CONFIGURATIONS
        CONFIG NAME single-process ARGS --force-single-process --single-app apps/test.configs.app/info.yaml
        CONFIG NAME multi-process CONDITION QT_FEATURE_am_multi_process ARGS --force-multi-process
        CONFIG NAME no-dbus CONDITION QT_FEATURE_am_multi_process ARGS -c am-config-nodbus.yaml --dbus none
        CONFIG NAME single-app CONDITION QT_FEATURE_am_multi_process ARGS --single-app apps/test.configs.app/info.yaml
        CONFIG NAME no-installer CONDITION QT_FEATURE_am_multi_process ARGS --disable-installer --wayland-socket-name wayland-am
)
