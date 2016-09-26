
load(fix-library-path)

LIBBACKTRACE_PATH = $$PWD/libbacktrace
LIBBACKTRACE_BUILD_PATH = $$shadowed($$LIBBACKTRACE_PATH)

INCLUDEPATH += $$LIBBACKTRACE_PATH
LIBS_PRIVATE += $$fixLibraryPath(-L$$LIBBACKTRACE_BUILD_PATH) -lqtbacktrace$$qtPlatformTargetSuffix()

CONFIG *= link_prl
