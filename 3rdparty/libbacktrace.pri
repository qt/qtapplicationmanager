
lessThan(QT_MINOR_VERSION, 8) {
    # this is the "old way" used by Qt <= 5.7
    load(fix-library-path)

    LIBBACKTRACE_PATH = $$PWD/libbacktrace
    LIBBACKTRACE_BUILD_PATH = $$shadowed($$LIBBACKTRACE_PATH)

    INCLUDEPATH += $$LIBBACKTRACE_PATH
    LIBS_PRIVATE += $$fixLibraryPath(-L$$LIBBACKTRACE_BUILD_PATH) -lqtbacktrace$$qtPlatformTargetSuffix()

    CONFIG *= link_prl
} else {
    # the "new way", starting with Qt 5.8
    QMAKE_USE_PRIVATE += backtrace
}
