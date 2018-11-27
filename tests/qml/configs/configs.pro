load(am-config)

AM_CONFIG = am-config.yaml
TEST_FILES = tst_configs.qml
TEST_CONFIGURATIONS = "--force-single-process" \
                      "--force-single-process --single-app apps/test.configs.app/info.yaml"
multi-process {
    TEST_CONFIGURATIONS += "--force-multi-process" \
                           "-c am-config-nodbus.yaml --dbus none" \
                           "--single-app apps/test.configs.app/info.yaml" \
                           "--disable-installer --wayland-socket-name wayland-am"
}

load(am-qml-testcase)
