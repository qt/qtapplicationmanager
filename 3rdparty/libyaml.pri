
!config_libyaml|no-system-libyaml {
    lessThan(QT_MINOR_VERSION, 8) {
        # this is the "old way" used by Qt <= 5.7
        load(fix-library-path)

        LIBYAML_PATH = $$PWD/libyaml
        LIBYAML_BUILD_PATH = $$shadowed($$LIBYAML_PATH)

        DEFINES *= YAML_DECLARE_STATIC
        INCLUDEPATH += $$LIBYAML_PATH/include
        LIBS_PRIVATE += $$fixLibraryPath(-L$$LIBYAML_BUILD_PATH) -lqtyaml$$qtPlatformTargetSuffix()

        CONFIG *= link_prl
    } else {
        # the "new way", starting with Qt 5.8
        QMAKE_USE_PRIVATE += yaml
    }
} else {
    PKGCONFIG_PRIVATE += "'yaml-0.1 >= 0.1.6'"
    CONFIG *= link_pkgconfig
}
