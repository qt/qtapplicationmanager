TEMPLATE = aux

OTHER_FILES += \
    *.prf \

#All the feature files we want to install and provide to other modules or to the examples
features.files += \
    am-systemui.prf
features.path = $$[QT_HOST_DATA]/mkspecs/features

INSTALLS += features
