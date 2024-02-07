
qt_internal_add_common_qt_library_helper(BundledLibDBus SHARED)

qt_internal_extend_target(BundledLibDBus
    SOURCES
        dbus/dbus.h
        dbus/dbus-auth.h
        dbus/dbus-auth.c
        dbus/dbus-address.h
        dbus/dbus-address.c
        dbus/dbus-bus.h
        dbus/dbus-bus.c
        dbus/dbus-connection-internal.h
        dbus/dbus-connection.h
        dbus/dbus-connection.c
        dbus/dbus-credentials.h
        dbus/dbus-credentials.c
        dbus/dbus-dataslot.h
        dbus/dbus-dataslot.c
        dbus/dbus-errors.h
        dbus/dbus-errors.c
        dbus/dbus-file.h
        dbus/dbus-file.c
        dbus/dbus-hash.h
        dbus/dbus-hash.c
        dbus/dbus-internals.h
        dbus/dbus-internals.c
        dbus/dbus-keyring.h
        dbus/dbus-keyring.c
        dbus/dbus-list.h
        dbus/dbus-list.c
        dbus/dbus-macros-internal.h
        dbus/dbus-macros.h
        dbus/dbus-marshal-basic.h
        dbus/dbus-marshal-basic.c
        dbus/dbus-marshal-header.h
        dbus/dbus-marshal-header.c
        dbus/dbus-marshal-byteswap.h
        dbus/dbus-marshal-byteswap.c
        dbus/dbus-marshal-recursive.h
        dbus/dbus-marshal-recursive.c
        dbus/dbus-marshal-validate.h
        dbus/dbus-marshal-validate.c
        dbus/dbus-mempool.h
        dbus/dbus-mempool.c
        dbus/dbus-memory.h
        dbus/dbus-memory.c
        dbus/dbus-message-internal.h
        dbus/dbus-message-private.h
        dbus/dbus-message.h
        dbus/dbus-message.c
        dbus/dbus-misc.h
        dbus/dbus-misc.c
        dbus/dbus-nonce.c
        dbus/dbus-object-tree.h
        dbus/dbus-object-tree.c
        dbus/dbus-pending-call.h
        dbus/dbus-pending-call.c
        dbus/dbus-pipe.h
        dbus/dbus-pipe.c
        dbus/dbus-protocol.h
        dbus/dbus-resources.h
        dbus/dbus-resources.c
        dbus/dbus-server.h
        dbus/dbus-server.c
        dbus/dbus-server-protected.h
        dbus/dbus-server-socket.c
        dbus/dbus-server-debug-pipe.h
        dbus/dbus-server-debug-pipe.c
        dbus/dbus-sha.h
        dbus/dbus-sha.c
        dbus/dbus-shared.h
        dbus/dbus-signature.h
        dbus/dbus-signature.c
        dbus/dbus-string-private.h
        dbus/dbus-string.h
        dbus/dbus-string.c
        dbus/dbus-syntax.h
        dbus/dbus-syntax.c
        dbus/dbus-sysdeps.h
        dbus/dbus-sysdeps.c
        dbus/dbus-timeout.h
        dbus/dbus-timeout.c
        dbus/dbus-threads-internal.h
        dbus/dbus-threads.h
        dbus/dbus-threads.c
        dbus/dbus-transport.h
        dbus/dbus-transport-protected.h
        dbus/dbus-transport.c
        dbus/dbus-transport-socket.c
        dbus/dbus-types.h
        dbus/dbus-uuidgen.h
        dbus/dbus-watch.h
        dbus/dbus-watch.c
    DEFINES
        DBUS_COMPILATION
        HAVE_CONFIG_H
        dbus_1_EXPORTS
    INCLUDE_DIRECTORIES
        dbus .
)

qt_internal_extend_target(BundledLibDBus CONDITION WIN32
    SOURCES
        dbus/dbus-backtrace-win.c
        dbus/dbus-file-win.c
        dbus/dbus-init-win.h
        dbus/dbus-init-win.cpp
        dbus/dbus-pipe-win.c
        dbus/dbus-server-win.c
        dbus/dbus-sysdeps-win.h
        dbus/dbus-sysdeps-win.c
        dbus/dbus-sysdeps-thread-win.c
        dbus/dbus-sockets-win.h
        dbus/dbus-transport-win.h
        dbus/dbus-transport-win.c
    DEFINES
        _CRT_NONSTDC_NO_DEPRECATE
        _CRT_SECURE_NO_DEPRECATE
        PLATFORM_CONFIG_H="config-windows.h"
        PLATFORM_DBUS_ARCH_DEPS_H="../dbus-arch-deps-windows.h"
    PUBLIC_LIBRARIES
        ws2_32 advapi32 netapi32 iphlpapi dbghelp
)

qt_internal_extend_target(BundledLibDBus CONDITION MACOS
    SOURCES
        dbus/dbus-file-unix.c
        dbus/dbus-pipe-unix.c
        dbus-server-launchd.h
        dbus-server-launchd.c
        dbus/dbus-server-unix.c
        dbus/dbus-sysdeps-unix.h
        dbus/dbus-sysdeps-unix.c
        dbus/dbus-sysdeps-pthread.c
        dbus/dbus-transport-unix.h
        dbus/dbus-transport-unix.h
        dbus/dbus-transport-unix.c
        dbus/dbus-userdb.h
        dbus/dbus-userdb.c
        dbus/dbus-uuidgen.c
    DEFINES
        PLATFORM_CONFIG_H="config-macos.h"
        PLATFORM_DBUS_ARCH_DEPS_H="../dbus-arch-deps-macos.h"
)

qt_disable_warnings(BundledLibDBus)
set_target_properties(BundledLibDBus PROPERTIES
    OUTPUT_NAME "dbus-1"
    DEBUG_POSTFIX ""                        # cmake should be using this for dylibs...
    FRAMEWORK_MULTI_CONFIG_POSTFIX_DEBUG "" # ...but is using that instead
    LIBRARY_OUTPUT_DIRECTORY "${QT_BUILD_DIR}/${INSTALL_LIBDIR}/qtapplicationmanager"
    RUNTIME_OUTPUT_DIRECTORY "${QT_BUILD_DIR}/${INSTALL_BINDIR}/qtapplicationmanager"
    ARCHIVE_OUTPUT_DIRECTORY "${QT_BUILD_DIR}/${INSTALL_LIBDIR}/qtapplicationmanager"
    VERSION 3
    SOVERSION 3
    _qt_module_skip_depends_include TRUE
)

qt_install(TARGETS BundledLibDBus
    RUNTIME DESTINATION "${INSTALL_BINDIR}/qtapplicationmanager"
    LIBRARY DESTINATION "${INSTALL_LIBDIR}/qtapplicationmanager"
    ARCHIVE DESTINATION "${INSTALL_LIBDIR}/qtapplicationmanager"
)
