# Generated from windowmapping.pro.

#####################################################################
## windowmapping Binary:
#####################################################################

qt_am_internal_add_qml_test(tst_windowmapping
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps
    TEST_FILE tst_windowmapping.qml
    CONFIGURATIONS #TODO: temporary - multi-process crashes in RHI on exit
        CONFIG NAME single-process ARGS --force-single-process
)

#### Keys ignored in scope 1:.:.:windowmapping.pro:<TRUE>:
# AM_CONFIG = "am-config.yaml"
# TEST_APPS = "test.winmap.amwin" "test.winmap.amwin2" "test.winmap.loader" "test.winmap.ping" "test.winmap.qtobject" "test.winmap.rectangle" "test.winmap.window"
# TEST_FILES = "tst_windowmapping.qml"
