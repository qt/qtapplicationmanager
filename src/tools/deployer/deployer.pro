
TEMPLATE = subdirs

load(am-config)

deployer.files = appman-deployer
deployer.path = $$INSTALL_PREFIX/bin/

INSTALLS = deployer

OTHER_FILES += \
    appman-deployer \
    deployer.qdoc \
