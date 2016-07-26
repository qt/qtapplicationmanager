
TEMPLATE = app
TARGET   = appman
DESTDIR  = $$BUILD_DIR/bin

load(am-config)

CONFIG *= console
QT = core network qml core-private
!headless:QT *= gui quick

qtHaveModule(dbus):QT *= dbus
qtHaveModule(pssdp):QT *= pssdp
qtHaveModule(pshellserver):QT *= pshellserver

DEFINES *= AM_BUILD_APPMAN

load(add-static-library)
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

win32:LIBS += -luser32

multi-process:!headless {
    qtHaveModule(waylandcompositor) {
        QT *= waylandcompositor waylandcompositor-private
        HEADERS += waylandcompositor.h
        SOURCES += waylandcompositor.cpp
    } else:qtHaveModule(compositor) {
        QT *= compositor
        HEADERS += waylandcompositor-old.h
        SOURCES += waylandcompositor-old.cpp
    }
}

target.path = $$INSTALL_PREFIX/bin/
INSTALLS += target

HEADERS += \
    qmllogger.h \
    configuration.h \

!headless:HEADERS += \
    inprocesswindow.h \
    waylandwindow.h \
    windowmanager.h \
    windowmanager_p.h \

SOURCES += \
    main.cpp \
    qmllogger.cpp \
    configuration.cpp \

!headless:SOURCES += \
    inprocesswindow.cpp \
    waylandwindow.cpp \
    windowmanager.cpp \


DBUS_ADAPTORS += \
    ../dbus/io.qt.applicationmanager.xml \
    ../dbus/io.qt.applicationinstaller.xml \

!headless:DBUS_ADAPTORS += \
    ../dbus/io.qt.windowmanager.xml \

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

recreate-applicationmanager-dbus-xml.CONFIG = phony
recreate-applicationmanager-dbus-xml.commands = $$QDBUSCPP2XML -a $$PWD/../manager-lib/applicationmanager.h -o $$PWD/../dbus/io.qt.applicationmanager.xml

recreate-applicationinstaller-dbus-xml.CONFIG = phony
recreate-applicationinstaller-dbus-xml.commands = $$QDBUSCPP2XML -a $$PWD/../installer-lib/applicationinstaller.h -o $$PWD/../dbus/io.qt.applicationinstaller.xml

recreate-windowmanager-dbus-xml.CONFIG = phony
recreate-windowmanager-dbus-xml.commands = $$QDBUSCPP2XML -a $$PWD/windowmanager.h -o $$PWD/../dbus/io.qt.windowmanager.xml

recreate-dbus-xml.depends = recreate-applicationmanager-dbus-xml recreate-applicationinstaller-dbus-xml

QMAKE_EXTRA_TARGETS += \
    recreate-dbus-xml \
    recreate-applicationmanager-dbus-xml \
    recreate-applicationinstaller-dbus-xml \
    recreate-windowmanager-dbus-xml \

OTHER_FILES = \
    syms.txt \

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

load(build-config)

unix:exists($$SOURCE_DIR/.git):GIT_VERSION=$$system(cd "$$SOURCE_DIR" && git describe --tags --always --dirty 2>/dev/null)
isEmpty(GIT_VERSION):GIT_VERSION="unknown"

createBuildConfig(_DATE_, VERSION, GIT_VERSION, SOURCE_DIR, BUILD_DIR, INSTALL_PREFIX, \
                  QT_ARCH, QT_VERSION, QT, CONFIG, DEFINES, \INCLUDEPATH, LIBS)
