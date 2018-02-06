
!config_libyaml|no-system-libyaml {
    QMAKE_USE_PRIVATE += yaml
} else {
    PKGCONFIG_PRIVATE += "'yaml-0.1 >= 0.1.6'"
    CONFIG *= link_pkgconfig
}
