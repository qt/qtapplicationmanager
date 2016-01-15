
TEMPLATE = subdirs

load(am-config)

deployer.files = application-deployer
deployer.path = $$INSTALL_PREFIX/bin/

INSTALLS = deployer

OTHER_FILES += \
    application-deployer
