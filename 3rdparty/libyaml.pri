
!config_libyaml|no-system-libyaml {
    load(fix-library-path)

    LIBYAML_PATH = $$PWD/libyaml
    LIBYAML_BUILD_PATH = $$shadowed($$LIBYAML_PATH)

    DEFINES *= YAML_DECLARE_STATIC
    INCLUDEPATH += $$LIBYAML_PATH/include
    LIBS_PRIVATE += $$fixLibraryPath(-L$$LIBYAML_BUILD_PATH) -lqtyaml$$qtPlatformTargetSuffix()

    CONFIG *= link_prl
} else {
    PKGCONFIG_PRIVATE += "'yaml-0.1 >= 0.1.6'"
    CONFIG *= link_pkgconfig
}
