TEMPLATE = app
TARGET   = appman

load(am-config)

QT = core network qml core-private
!headless:QT *= gui quick
qtHaveModule(dbus):QT *= dbus
qtHaveModule(pssdp):QT *= pssdp
qtHaveModule(pshellserver):QT *= pshellserver
QT *= \
    appman_common-private \
    appman_application-private \
    appman_manager-private \
    appman_installer-private \
    appman_notification-private \

CONFIG *= console

win32:LIBS += -luser32

multi-process:!headless {
    qtHaveModule(waylandcompositor) {
        QT *= waylandcompositor waylandcompositor-private
        HEADERS += waylandcompositor.h
        SOURCES += waylandcompositor.cpp
        PKGCONFIG += wayland-server
    } else:qtHaveModule(compositor) {
        QT *= compositor
        HEADERS += waylandcompositor-old.h
        SOURCES += waylandcompositor-old.cpp
    }
}

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
    ../dbus/io.qt.applicationinstaller.xml \

!headless:DBUS_ADAPTORS += \
    ../dbus/io.qt.windowmanager.xml \

# this is a bit more complicated than it should be, but qdbusxml2cpp cannot
# cope with more than 1 out value out of the box
# http://lists.qt-project.org/pipermail/interest/2013-July/008011.html
dbus-notifications.files = ../dbus/org.freedesktop.notifications.xml
dbus-notifications.source_flags = -l QtAM::NotificationManager
dbus-notifications.header_flags = -l QtAM::NotificationManager -i notificationmanager.h

dbus-appman.files = ../dbus/io.qt.applicationmanager.xml
dbus-appman.source_flags = -l QtAM::ApplicationManager
dbus-appman.header_flags = -l QtAM::ApplicationManager -i applicationmanager.h

DBUS_ADAPTORS += dbus-notifications dbus-appman

load(qt_tool)

load(install-prefix)

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
                  QT_ARCH, QT_VERSION, QT, CONFIG, DEFINES, INCLUDEPATH, LIBS)
