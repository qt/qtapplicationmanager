TEMPLATE = lib
TARGET = QtAppManNotification
MODULE = appman_notification

load(am-config)

QT = core
qtHaveModule(qml):QT *= qml
QT_FOR_PRIVATE *= appman_common-private

CONFIG *= static internal_module
CONFIG -= create_cmake

HEADERS += \
    notification.h \

SOURCES += \
    notification.cpp \

load(qt_module)
