TEMPLATE = app
TARGET   = appman

include(manager.pri)

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
