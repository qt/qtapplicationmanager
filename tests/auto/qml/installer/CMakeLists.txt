# Generated from installer.pro.

#####################################################################
## installer Binary:
#####################################################################

qt_am_internal_add_qml_test(tst_installer
    CONFIG_YAML am-config.yaml
    EXTRA_FILES apps
    TESTDATA_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../../data
    TEST_FILE tst_installer.qml
)

#### Keys ignored in scope 1:.:.:installer.pro:<TRUE>:
# AM_CONFIG = "am-config.yaml"
# AM_TESTDATA_DIR = "\"$$PWD/../../data/\""
# TEST_FILES = "tst_installer.qml"
