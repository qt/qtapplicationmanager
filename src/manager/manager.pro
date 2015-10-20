
TEMPLATE = app
TARGET   = appman
DESTDIR  = $$BUILD_DIR/bin

include($$BASE_PRI)

CONFIG *= console
QT = core network qml core-private
!headless:QT *= gui quick

qtHaveModule(dbus):QT *= dbus
qtHaveModule(pssdp):QT *= pssdp
qtHaveModule(pshellserver):QT *= pshellserver

DEFINES *= AM_BUILD_APPMAN

addStaticLibrary(../common-lib)
addStaticLibrary(../manager-lib)
addStaticLibrary(../installer-lib)
addStaticLibrary(../notification-lib)

# We need to export some symbols for the plugins, but by default none are exported
# for executables. We can either export all symbols, or specify a list.
# On Windows __declspec also works for executables, but __attribute__((visibility))
# has no effect on Unix, so we are either stuck with exporting all symbols, or
# maintaining a list of exported symbol names:
#LIBS += -Wl,-E  # all
unix:!osx:!android:LIBS += -Wl,--dynamic-list=$$PWD/syms.txt  # sub set

force-singleprocess|!qtHaveModule(compositor) {
    DEFINES *= AM_SINGLEPROCESS_MODE
} else:qtHaveModule(compositor) {
    QT *= compositor
}

target.path = $$INSTALL_PREFIX/bin/
INSTALLS += target

HEADERS += \
    qmllogger.h \
    configuration.h

!headless:HEADERS += \
    inprocesswindow.h \
    waylandwindow.h \
    windowmanager.h \

SOURCES += \
    main.cpp \
    qmllogger.cpp \
    configuration.cpp

!headless:SOURCES += \
    inprocesswindow.cpp \
    waylandwindow.cpp \
    windowmanager.cpp \


DBUS_ADAPTORS += \
    ../dbus/com.pelagicore.applicationmanager.xml \
    ../dbus/com.pelagicore.applicationinstaller.xml \

# this is a bit more complicated than it should be, but qdbusxml2cpp cannot
# cope with more than 1 out value out of the box
# http://lists.qt-project.org/pipermail/interest/2013-July/008011.html
dbus-notifications.files = ../dbus/org.freedesktop.notifications.xml
dbus-notifications.source_flags = -l NotificationManager
dbus-notifications.header_flags = -l NotificationManager -i notificationmanager.h

DBUS_ADAPTORS += dbus-notifications

dbusif.path = $$INSTALL_PREFIX/share/dbus-1/interfaces/
dbusif.files = $$DBUS_ADAPTORS
INSTALLS += dbusif

qtPrepareTool(QDBUSCPP2XML, qdbuscpp2xml)

recreate_manager_dbus_xml.target = $$PWD/../dbus/com.pelagicore.applicationmanager.xml
recreate_manager_dbus_xml.CONFIG = phony
recreate_manager_dbus_xml.commands = $$QDBUSCPP2XML -a $$PWD/../manager-lib/applicationmanager.h -o $$recreate_manager_dbus_xml.target

recreate_installer_dbus_xml.target = $$PWD/../dbus/com.pelagicore.applicationinstaller.xml
recreate_installer_dbus_xml.CONFIG = phony
recreate_installer_dbus_xml.commands = $$QDBUSCPP2XML -a $$PWD/../installer-lib/applicationinstaller.h -o $$recreate_installer_dbus_xml.target

recreate-dbus-xml.depends = recreate_manager_dbus_xml recreate_installer_dbus_xml

QMAKE_EXTRA_TARGETS += recreate-dbus-xml recreate_manager_dbus_xml recreate_installer_dbus_xml


OTHER_FILES = \
    syms.txt \

maliit {
    packagesExist(maliit-server maliit-connection) {
        DEFINES += MALIIT_INTEGRATION
        CONFIG *= link_pkgconfig
        PKGCONFIG *= maliit-server
    } else {
        error("You required Maliit, though it does not seem to be installed")
    }
}

android {
    QT *= androidextras

    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android

    DISTFILES += \
        android/gradle/wrapper/gradle-wrapper.jar \
        android/AndroidManifest.xml \
        android/gradlew.bat \
        android/res/values/libs.xml \
        android/build.gradle \
        android/gradle/wrapper/gradle-wrapper.properties \
        android/gradlew

    # hack for neptune to get the plugin deployed
#    ANDROID_EXTRA_LIBS = $$[QT_INSTALL_QML]/com/pelagicore/datasource/libqmldatasources.so

    # hack for neptune to get all relevant Qt modules deployed
#    DISTFILES += android-deploy-dummy.qml
}
