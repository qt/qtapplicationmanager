# Generated from simple.pro.

#####################################################################
## simple Binary:
#####################################################################

qt_am_internal_add_qml_test(tst_simple
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps
    TEST_FILE tst_applicationmanager.qml
)

#### Keys ignored in scope 1:.:.:simple.pro:<TRUE>:
# AM_CONFIG = "am-config.yaml"
# TEST_APPS = "tld.test.simple1" "tld.test.simple2"
# TEST_FILES = "tst_applicationmanager.qml"
