load(am-config)

AM_CONFIG = am-config.yaml
TEST_FILES = tst_processtitle.qml
TEST_APPS = test.processtitle.app

multi-process {
     TEST_CONFIGURATIONS += "--force-multi-process" \
                            "--force-multi-process -c $$_PRO_FILE_PWD_/am-config-quick.yaml"
}

load(am-qml-testcase)
