TEMPLATE = aux

deployer.files = appman-deployer
deployer.path = $$[QT_HOST_BINS]

INSTALLS = deployer

OTHER_FILES += \
    appman-deployer \
    deployer.qdoc \
