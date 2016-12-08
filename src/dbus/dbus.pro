TEMPLATE = aux

OTHER_FILES = \
    io.qt.applicationinstaller.xml \
    io.qt.applicationmanager.applicationinterface.xml \
    io.qt.applicationmanager.runtimeinterface.xml \
    io.qt.applicationmanager.xml \
    io.qt.windowmanager.xml \
    org.freedesktop.notifications.xml \

qtPrepareTool(QDBUSCPP2XML, qdbuscpp2xml)

recreate-applicationmanager-dbus-xml.CONFIG = phony
recreate-applicationmanager-dbus-xml.commands = $$QDBUSCPP2XML -a $$PWD/../manager-lib/applicationmanager.h -o $$PWD/io.qt.applicationmanager.xml

recreate-applicationinstaller-dbus-xml.CONFIG = phony
recreate-applicationinstaller-dbus-xml.commands = $$QDBUSCPP2XML -a $$PWD/../installer-lib/applicationinstaller.h -o $$PWD/io.qt.applicationinstaller.xml

recreate-windowmanager-dbus-xml.CONFIG = phony
recreate-windowmanager-dbus-xml.commands = $$QDBUSCPP2XML -a $$PWD/../manager/windowmanager.h -o $$PWD/io.qt.windowmanager.xml

recreate-dbus-xml.depends = recreate-applicationmanager-dbus-xml recreate-applicationinstaller-dbus-xml recreate-windowmanager-dbus-xml

QMAKE_EXTRA_TARGETS += \
    recreate-dbus-xml \
    recreate-applicationmanager-dbus-xml \
    recreate-applicationinstaller-dbus-xml \
    recreate-windowmanager-dbus-xml \
