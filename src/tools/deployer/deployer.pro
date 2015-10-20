
TEMPLATE = subdirs

include($$BASE_PRI)

deployer.files = application-deployer
deployer.path = $$INSTALL_PREFIX/bin/

INSTALLS = deployer

OTHER_FILES += \
    application-deployer
