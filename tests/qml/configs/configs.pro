load(am-config)

AM_CONFIG = am-config.yaml
TEST_FILES = tst_configs.qml
TEST_APPS = test.configs.app
TEST_CONFIGURATIONS = "--force-single-process" \
                      "--force-single-process --single-app $$_PRO_FILE_PWD_/apps/test.configs.app/info.yaml"
multi-process {
    TEST_CONFIGURATIONS += "--force-multi-process" \
                           "-c $$_PRO_FILE_PWD_/am-config-nodbus.yaml --dbus none" \
                           "--single-app $$_PRO_FILE_PWD_/apps/test.configs.app/info.yaml" \
                           "--disable-installer --wayland-socket-name wayland-am"
}

load(am-qml-testcase)
