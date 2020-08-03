load(am-config)

AM_CONFIG = am-config.yaml
TEST_FILES = tst_intents.qml
TEST_APPS = intents1 intents2 cannot-start
TEST_CONFIGURATIONS = "--force-single-process"
multi-process {
    TEST_CONFIGURATIONS += "--force-multi-process" \
                           "--force-multi-process -c $$_PRO_FILE_PWD_/am-config-quick.yaml"
}

load(am-qml-testcase)
