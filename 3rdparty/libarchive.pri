
include(libz.pri)

!config_libarchive|no-system-libarchive {
    QMAKE_USE_PRIVATE += archive
} else {
    PKGCONFIG_PRIVATE += "'libarchive >= 3.1.2'"
    CONFIG *= link_pkgconfig
}
