TEMPLATE = app
TARGET   = appman-packager

load(am-config)

QT = core network
QT *= \
    appman_common-private \
    appman_crypto-private \
    appman_application-private \
    appman_package-private \

CONFIG *= console

SOURCES += \
    packager.cpp \
    packagingjob.cpp \

HEADERS += \
    packagingjob.h \

load(qt_tool)

load(install-prefix)
