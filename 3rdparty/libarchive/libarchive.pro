TEMPLATE = lib
TARGET = qtarchive

load(am-config)

CONFIG += \
    static \
    hide_symbols \
    exceptions_off rtti_off warn_off \
    installed

macos:LIBS += -framework CoreServices -liconv
ios:LIBS += -liconv
win32:LIBS += -lcrypt32
win32:MODULE_DEFINES += LIBARCHIVE_STATIC
MODULE_INCLUDEPATH += $$PWD/libarchive

load(qt_helper_lib)

win32-msvc* {
    QMAKE_CFLAGS += /wd4146 /wd4133 /D_CRT_SECURE_NO_WARNINGS
}
*-g++* {
    QMAKE_CFLAGS += -Wno-unused -Wno-sign-compare -Wno-old-style-declaration
}
*-clang* {
    CONFIG *= warn_off
    QMAKE_CFLAGS += -Wall -W -Wno-unused -Wno-sign-compare
}

android:DEFINES += PLATFORM_CONFIG_H=\\\"config-android.h\\\"
else:win32:DEFINES += PLATFORM_CONFIG_H=\\\"config-windows.h\\\"
else:macos:DEFINES += PLATFORM_CONFIG_H=\\\"config-macos.h\\\"
else:ios:DEFINES += PLATFORM_CONFIG_H=\\\"config-ios.h\\\"
else:DEFINES += PLATFORM_CONFIG_H=\\\"config-unix.h\\\"

OTHER_FILES += \
    config-android.h \
    config-windows.h \
    config-macos.h \
    config-ios.h \
    config-unix.h \
    android_lf.h \

INCLUDEPATH *= $$PWD/libarchive

include(../libz.pri)

# disabled for now, since we have 2 problems:
#  1) the python/django appstore is based on python 2.7 which does not support it via tarfile
#  2) we get a weird error on macOS when creating XZ'ed packages from libarchive
# include(../liblzma.pri)

SOURCES += \
    libarchive/archive_acl.c \
    libarchive/archive_check_magic.c \
    libarchive/archive_cmdline.c \
    libarchive/archive_entry.c \
    libarchive/archive_entry_copy_stat.c \
    libarchive/archive_entry_link_resolver.c \
    libarchive/archive_entry_sparse.c \
    libarchive/archive_entry_stat.c \
    libarchive/archive_entry_strmode.c \
    libarchive/archive_entry_xattr.c \
    libarchive/archive_getdate.c \
    libarchive/archive_match.c \
    libarchive/archive_options.c \
    libarchive/archive_pathmatch.c \
    libarchive/archive_ppmd7.c \
    libarchive/archive_random.c \
    libarchive/archive_rb.c \
    libarchive/archive_read_append_filter.c \
    libarchive/archive_read.c \
    libarchive/archive_read_data_into_fd.c \
    libarchive/archive_read_disk_entry_from_file.c \
    libarchive/archive_read_disk_set_standard_lookup.c \
    libarchive/archive_read_extract.c \
    libarchive/archive_read_open_fd.c \
    libarchive/archive_read_open_file.c \
    libarchive/archive_read_open_filename.c \
    libarchive/archive_read_open_memory.c \
    libarchive/archive_read_set_format.c \
    libarchive/archive_read_set_options.c \
    libarchive/archive_read_support_filter_bzip2.c \
    libarchive/archive_read_support_filter_gzip.c \
    libarchive/archive_read_support_filter_none.c \
    libarchive/archive_read_support_filter_program.c \
    libarchive/archive_read_support_filter_xz.c \
    libarchive/archive_read_support_format_by_code.c \
    libarchive/archive_read_support_format_empty.c \
    libarchive/archive_read_support_format_tar.c \
    libarchive/archive_string.c \
    libarchive/archive_string_sprintf.c \
    libarchive/archive_util.c \
    libarchive/archive_virtual.c \
    libarchive/archive_write_add_filter_b64encode.c \
    libarchive/archive_write_add_filter_by_name.c \
    libarchive/archive_write_add_filter_bzip2.c \
    libarchive/archive_write_add_filter.c \
    libarchive/archive_write_add_filter_gzip.c \
    libarchive/archive_write_add_filter_none.c \
    libarchive/archive_write_add_filter_program.c \
    libarchive/archive_write_add_filter_xz.c \
    libarchive/archive_write.c \
    libarchive/archive_write_disk_set_standard_lookup.c \
    libarchive/archive_write_open_fd.c \
    libarchive/archive_write_open_file.c \
    libarchive/archive_write_open_filename.c \
    libarchive/archive_write_open_memory.c \
    libarchive/archive_write_set_format_by_name.c \
    libarchive/archive_write_set_format.c \
    libarchive/archive_write_set_format_gnutar.c \
    libarchive/archive_write_set_format_ustar.c \
    libarchive/archive_write_set_options.c \

!win32:SOURCES += \
    libarchive/archive_read_disk_posix.c \
    libarchive/archive_write_disk_posix.c \
    libarchive/filter_fork_posix.c \

win32:SOURCES += \
    libarchive/archive_entry_copy_bhfi.c \
    libarchive/archive_read_disk_windows.c \
    libarchive/archive_windows.c \
    libarchive/archive_write_disk_windows.c \
    libarchive/filter_fork_windows.c \
