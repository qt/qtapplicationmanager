
bundled-libyaml {
    LIBYAML_PATH = $$PWD/libyaml
    LIBYAML_BUILD_PATH = $$shadowed($$LIBYAML_PATH)

    DEFINES *= YAML_DECLARE_STATIC
    INCLUDEPATH += $$LIBYAML_PATH/include
    LIBS += $$fixLibraryPath(-L$$LIBYAML_BUILD_PATH) -lyaml

    CONFIG *= link_prl
} else {
    PKGCONFIG += "'yaml-0.1 >= 0.1.6'"
}
