# Generated from libarchive.pro.

#####################################################################
## BundledArchive Generic Library:
#####################################################################

if (APPLE)
    find_package(Iconv)
endif()

qt_internal_add_3rdparty_library(BundledLibArchive
    QMAKE_LIB_NAME archive
    STATIC
    INSTALL
    SOURCES
        libarchive/archive_acl.c
        libarchive/archive_check_magic.c
        libarchive/archive_cmdline.c
        libarchive/archive_entry.c
        libarchive/archive_entry_copy_stat.c
        libarchive/archive_entry_link_resolver.c
        libarchive/archive_entry_sparse.c
        libarchive/archive_entry_stat.c
        libarchive/archive_entry_strmode.c
        libarchive/archive_entry_xattr.c
        libarchive/archive_getdate.c
        libarchive/archive_match.c
        libarchive/archive_options.c
        libarchive/archive_pathmatch.c
        libarchive/archive_ppmd7.c
        libarchive/archive_random.c
        libarchive/archive_rb.c
        libarchive/archive_read.c
        libarchive/archive_read_append_filter.c
        libarchive/archive_read_data_into_fd.c
        libarchive/archive_read_disk_entry_from_file.c
        libarchive/archive_read_disk_set_standard_lookup.c
        libarchive/archive_read_extract.c
        libarchive/archive_read_open_fd.c
        libarchive/archive_read_open_file.c
        libarchive/archive_read_open_filename.c
        libarchive/archive_read_open_memory.c
        libarchive/archive_read_set_format.c
        libarchive/archive_read_set_options.c
        libarchive/archive_read_support_filter_bzip2.c
        libarchive/archive_read_support_filter_gzip.c
        libarchive/archive_read_support_filter_none.c
        libarchive/archive_read_support_filter_program.c
        libarchive/archive_read_support_filter_xz.c
        libarchive/archive_read_support_format_by_code.c
        libarchive/archive_read_support_format_empty.c
        libarchive/archive_read_support_format_tar.c
        libarchive/archive_string.c
        libarchive/archive_string_sprintf.c
        libarchive/archive_util.c
        libarchive/archive_virtual.c
        libarchive/archive_write.c
        libarchive/archive_write_add_filter.c
        libarchive/archive_write_add_filter_b64encode.c
        libarchive/archive_write_add_filter_by_name.c
        libarchive/archive_write_add_filter_bzip2.c
        libarchive/archive_write_add_filter_gzip.c
        libarchive/archive_write_add_filter_none.c
        libarchive/archive_write_add_filter_program.c
        libarchive/archive_write_add_filter_xz.c
        libarchive/archive_write_disk_set_standard_lookup.c
        libarchive/archive_write_open_fd.c
        libarchive/archive_write_open_file.c
        libarchive/archive_write_open_filename.c
        libarchive/archive_write_open_memory.c
        libarchive/archive_write_set_format.c
        libarchive/archive_write_set_format_by_name.c
        libarchive/archive_write_set_format_gnutar.c
        libarchive/archive_write_set_format_ustar.c
        libarchive/archive_write_set_options.c
    INCLUDE_DIRECTORIES
        libarchive
        .
    PUBLIC_INCLUDE_DIRECTORIES
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libarchive>
)
qt_disable_warnings(BundledLibArchive)
qt_set_symbol_visibility_hidden(BundledLibArchive)

qt_internal_extend_target(BundledLibArchive CONDITION MACOS
    DEFINES
        PLATFORM_CONFIG_H=\\\"config-macos.h\\\"
    PUBLIC_LIBRARIES
        ${FWCoreServices}
        Iconv::Iconv
)

qt_internal_extend_target(BundledLibArchive CONDITION IOS
    PUBLIC_LIBRARIES
        Iconv::Iconv
)

qt_internal_extend_target(BundledLibArchive CONDITION WIN32
    SOURCES
        libarchive/archive_entry_copy_bhfi.c
        libarchive/archive_read_disk_windows.c
        libarchive/archive_windows.c
        libarchive/archive_write_disk_windows.c
        libarchive/filter_fork_windows.c
    DEFINES
        PLATFORM_CONFIG_H=\\\"config-windows.h\\\"
    PUBLIC_DEFINES
        LIBARCHIVE_STATIC
    PUBLIC_LIBRARIES
        advapi32
        crypt32
)

#### Keys ignored in scope 6:.:.:libarchive.pro:win32-msvc_x_:
# QMAKE_CFLAGS = "/wd4146" "/wd4133" "/D_CRT_SECURE_NO_WARNINGS"

#### Keys ignored in scope 7:.:.:libarchive.pro:GCC:
# QMAKE_CFLAGS = "-Wno-unused" "-Wno-sign-compare" "-Wno-old-style-declaration"

#### Keys ignored in scope 8:.:.:libarchive.pro:CLANG:
# QMAKE_CFLAGS = "-Wall" "-W" "-Wno-unused" "-Wno-sign-compare"

qt_internal_extend_target(BundledLibArchive CONDITION ANDROID
    DEFINES
        PLATFORM_CONFIG_H=\\\"config-android.h\\\"
)

qt_internal_extend_target(BundledLibArchive CONDITION IOS AND NOT MACOS
    DEFINES
        PLATFORM_CONFIG_H=\\\"config-ios.h\\\"
)

qt_internal_extend_target(BundledLibArchive CONDITION UNIX AND NOT ANDROID AND NOT IOS AND NOT MACOS
    DEFINES
        PLATFORM_CONFIG_H=\\\"config-unix.h\\\"
)

qt_internal_extend_target(BundledLibArchive CONDITION UNIX
    SOURCES
        libarchive/archive_read_disk_posix.c
        libarchive/archive_write_disk_posix.c
        libarchive/filter_fork_posix.c
)

if (QT_FEATURE_system_zlib)
    find_package(ZLIB)

    qt_internal_extend_target(BundledLibArchive
        LIBRARIES
            ZLIB::ZLIB
    )
else()
    qt_internal_extend_target(BundledLibArchive
        LIBRARIES
            Qt::Core
            Qt::ZlibPrivate
    )
endif()
