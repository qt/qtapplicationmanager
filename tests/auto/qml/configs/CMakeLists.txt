# Generated from configs.pro.

#####################################################################
## configs Binary:
#####################################################################

qt_internal_add_executable(configs
    GUI
    PUBLIC_LIBRARIES
        Qt::Gui
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
