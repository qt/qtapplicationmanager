
bundled-libyaml {
    error("bundled-libyaml is not supported ATM")

#    LIBYAML_PATH = $$PWD/libyaml-0.1.6
#    LIBYAML_BUILD_PATH = $$shadowed($$LIBYAML_PATH)
#
#    DEFINES *= YAML_DECLARE_STATIC
#    INCLUDEPATH += $$LIBYAML_PATH/include
#    LIBS += $$fixLibraryPath(-L$$LIBYAML_BUILD_PATH) -lyaml
#
#    CONFIG *= link_prl
} else {
    osx:exists(/usr/local/bin/pkg-config):PKG_CONFIG=/usr/local/bin/pkg-config

    PKGCONFIG += yaml-0.1
}
