# Generated from intents.pro.

#####################################################################
## intents Binary:
#####################################################################

qt_am_internal_add_qml_test(tst_intents
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps am-config-quick.yaml
    TEST_FILE tst_intents.qml
    CONFIGURATIONS
        CONFIG NAME single-process ARGS --force-single-process
        CONFIG NAME multi-process CONDITION FEATURE_am_multi_process ARGS --force-multi-process
        CONFIG NAME config-quick CONDITION FEATURE_am_multi_process ARGS --force-multi-process -c am-config-quick.yaml
)

#### Keys ignored in scope 1:.:.:intents.pro:<TRUE>:
# AM_CONFIG = "am-config.yaml"
# TEST_APPS = "intents1" "intents2" "cannot-start"
# TEST_CONFIGURATIONS = "--force-single-process"
# TEST_FILES = "tst_intents.qml"

## Scopes:
#####################################################################

#### Keys ignored in scope 2:.:.:intents.pro:multi-process:
# TEST_CONFIGURATIONS = "--force-multi-process" "--force-multi-process -c $$_PRO_FILE_PWD_/am-config-quick.yaml"
