TEMPLATE = aux

CONFIG += nostrip

deployer.files = appman-deployer
deployer.path = $$[QT_HOST_BINS]

INSTALLS = deployer

OTHER_FILES += \
    appman-deployer \

load(install-prefix)
