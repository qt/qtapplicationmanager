requires(linux:!android|win32-msvc2013:!winrt|win32-msvc2015:!winrt|osx|win32-g++*)

!tools-only:!qtHaveModule(qml):error("The QtQml library is required for a non 'tools-only' build")

TEMPLATE = subdirs
CONFIG += ordered

enable-tests:QT_BUILD_PARTS *= tests
else:contains(QT_BUILD_PARTS, "tests"):CONFIG += enable-tests
enable-examples:QT_BUILD_PARTS *= examples
else:contains(QT_BUILD_PARTS, "examples"):CONFIG += enable-examples

load(configure)
qtCompileTest(libarchive)
qtCompileTest(libyaml)
qtCompileTest(libdbus)

qtHaveModule(compositor)|if(qtHaveModule(waylandcompositor):qtHaveModule(waylandcompositor-private)) {
    CONFIG += am_compatible_compositor
}

force-single-process:force-multi-process:error("You cannot both specify force-single-process and force-multi-process")
force-multi-process:!headless:!am_compatible_compositor:error("You forced multi-process mode, but the QtCompositor module is not available")
force-multi-process:!config_libdbus:error("You forced multi-process mode, but libdbus-1 (>= 1.6) is not available")

if(linux|force-libcrypto) {
    !if(contains(QT_CONFIG,"openssl")|contains(QT_CONFIG,"openssl-linked")):error("Qt was built without OpenSSL support.")
}

MIN_MINOR=6

!equals(QT_MAJOR_VERSION, 5)|lessThan(QT_MINOR_VERSION, $$MIN_MINOR):error("This application needs to be built against Qt 5.$${MIN_MINOR}+")

load(am-config)

!config_libyaml|no-system-libyaml {
    force-system-libyaml:error("Could not find a system installation for libyaml.")
    else:SUBDIRS += 3rdparty/libyaml/libyaml.pro
}
!config_libarchive|no-system-libarchive {
    force-system-libarchive:error("Could not find a system installation for libarchive.")
    else:SUBDIRS += 3rdparty/libarchive/libarchive.pro
}

linux:!disable-libbacktrace:if(enable-libbacktrace|CONFIG(debug, debug|release))  {
    check_libbacktrace = "yes"
    SUBDIRS += 3rdparty/libbacktrace/libbacktrace.pro
} else {
    check_libbacktrace = "no"
}

!tools-only: SUBDIRS += doc dummyimports

load(qt_parts)

tools-only {
    # removing them from QT_BUILD_PARTS doesn't help
    SUBDIRS -= sub_tests
    SUBDIRS -= sub_examples
}

if(linux|force-libcrypto):check_crypto = "libcrypto / OpenSSL"
else:win32:check_crypto = "WinCrypt"
else:osx:check_crypto = "SecurityFramework"

auto-multi-process { check_multi = "yes (auto detect)" } else { check_multi = "no (auto detect)" }
force-single-process { check_multi = "no (force-single-process)" }
force-multi-process  { check_multi = "yes (force-multi-process)" }

CONFIG(debug, debug|release) { check_debug = "debug" } else { check_debug = "release" }

!isEmpty(AM_HARDWARE_ID_FF):check_hwid = "(from file) $$AM_HARDWARE_ID_FF"
else:!isEmpty(AM_HARDWARE_ID):check_hwid = "$$AM_HARDWARE_ID"
else:check_hwid = "auto (derived from network MAC address)"

load(config-output)

printConfigLine()
printConfigLine("-- Application Manager configuration", , orange)
printConfigLine("Version", "$$MODULE_VERSION", white)
printConfigLine("Hardware Id", $$check_hwid, auto)
printConfigLine("Qt version", $$[QT_VERSION], white)
printConfigLine("Qt installation", $$[QT_HOST_BINS])
printConfigLine("Debug or release", $$check_debug, white)
printConfigLine("Installation prefix", $$INSTALL_PREFIX, auto)
printConfigLine("Tools only build", $$yesNo(CONFIG(tools-only)), no)
printConfigLine("Headless", $$yesNo(CONFIG(headless)), auto)
printConfigLine("QtCompositor support", $$yesNo(CONFIG(am_compatible_compositor)), auto)
printConfigLine("Multi-process mode", $$check_multi, auto)
printConfigLine("Installer enabled", $$yesNo(!CONFIG(disable-installer)), auto)
printConfigLine("Tests enabled", $$yesNo(CONFIG(enable-tests)), auto)
printConfigLine("Examples enabled", $$yesNo(CONFIG(enable-examples)), auto)
printConfigLine("Crypto backend", $$check_crypto, auto)
printConfigLine("SSDP support", $$yesNo(qtHaveModule(pssdp)), auto)
printConfigLine("Shellserver support", $$yesNo(qtHaveModule(pshellserver)), auto)
printConfigLine("Genivi support", $$yesNo(qtHaveModule(geniviextras)), auto)
printConfigLine("libbacktrace support", $$check_libbacktrace, auto)
printConfigLine("Systemd workaround", $$yesNo(CONFIG(systemd-workaround)), auto)
printConfigLine("System libarchive", $$yesNo(config_libarchive:!no-system-libarchive), auto)
printConfigLine("System libyaml", $$yesNo(config_libyaml:!no-system-libyaml), auto)
printConfigLine()

OTHER_FILES += \
    INSTALL.md \
    .qmake.conf \
    application-manager.conf \
    template-opt/am/*.yaml \
    qmake-features/*.prf \
    sync.profile \
    header.*[^~] \
    LICENSE.*[^~] \
    config.tests/libarchive/* \
    config.tests/libdbus/* \
    config.tests/libyaml/* \

GCOV_EXCLUDE = /usr/* \
               $$[QT_INSTALL_PREFIX]/* \
               $$[QT_INSTALL_PREFIX/src]/* \
               tests/* \
               moc_* \
               examples/* \
               $$OUT_PWD/* \


!prefix_build: GCOV_EXCLUDE += $$clean_path($$[QT_INSTALL_PREFIX]/../*) $$clean_path($$[QT_INSTALL_PREFIX/src]/../*)

for (f, GCOV_EXCLUDE) {
    GCOV_EXCLUDE_STR += $$shell_quote($$f)
}

global-check-coverage.target = check-coverage
global-check-coverage.depends = coverage
global-check-coverage.commands = ( \
    find . -name \"*.gcov-info\" -print0 | xargs -0 rm -f && \
    lcov -c -i -d . --rc lcov_branch_coverage=1 --rc geninfo_auto_base=1 -o $$OUT_PWD/base.gcov-info && \
    cd tests && make check && cd .. && \
    lcov -c -d . --rc lcov_branch_coverage=1 --rc geninfo_auto_base=1 -o $$OUT_PWD/test.gcov-info && \
    lcov --rc lcov_branch_coverage=1 -o $$OUT_PWD/temp.gcov-info `find . -name \"*.gcov-info\" | xargs -n1 echo -a` && \
    lcov --rc lcov_branch_coverage=1 -o $$OUT_PWD/application-manager.gcov-info -r temp.gcov-info $$GCOV_EXCLUDE_STR && \
    rm -f base.gcov-info test.gcov-info temp.gcov-info && \
    genhtml -o branch-coverage -s -f --legend --branch-coverage --rc lcov_branch_coverage=1 --demangle-cpp application-manager.gcov-info && echo \"\\n\\nCoverage info is available at file://`pwd`/branch-coverage/index.html\" \
)

QMAKE_EXTRA_TARGETS -= sub-check-coverage
QMAKE_EXTRA_TARGETS *= global-check-coverage
