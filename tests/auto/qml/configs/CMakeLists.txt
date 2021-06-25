# Generated from configs.pro.

#####################################################################
## configs Binary:
#####################################################################

qt_am_internal_add_qml_test(tst_configs
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps am-config-nodbus.yaml
    TEST_FILE tst_configs.qml
    CONFIGURATIONS
        CONFIG NAME single-process ARGS --force-single-process --single-app apps/test.configs.app/info.yaml
        CONFIG NAME multi-process CONDITION FEATURE_am_multi_process ARGS --force-multi-process
        CONFIG NAME no-dbus CONDITION FEATURE_am_multi_process ARGS -c am-config-nodbus.yaml --dbus none
        CONFIG NAME single-app CONDITION FEATURE_am_multi_process ARGS --single-app apps/test.configs.app/info.yaml
        CONFIG NAME no-installer CONDITION FEATURE_am_multi_process ARGS --disable-installer --wayland-socket-name wayland-am
)

#### Keys ignored in scope 1:.:.:configs.pro:<TRUE>:
# AM_CONFIG = "am-config.yaml"
# TEST_APPS = "test.configs.app"
# TEST_CONFIGURATIONS = "--force-single-process" "--force-single-process --single-app $$_PRO_FILE_PWD_/apps/test.configs.app/info.yaml"
# TEST_FILES = "tst_configs.qml"

## Scopes:
#####################################################################

#### Keys ignored in scope 2:.:.:configs.pro:multi-process:
# TEST_CONFIGURATIONS = "--force-multi-process" "-c $$_PRO_FILE_PWD_/am-config-nodbus.yaml --dbus none" "--single-app $$_PRO_FILE_PWD_/apps/test.configs.app/info.yaml" "--disable-installer --wayland-socket-name wayland-am"
