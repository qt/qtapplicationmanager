
lessThan(QT_MAJOR_VERSION, 5):error("This application needs to be built against Qt 5")
win32-msvc2010:error("This application needs to be built with at least MSVC 2012")

CONFIG *= c++11
CONFIG *= link_pkgconfig
CONFIG *= no_private_qt_headers_warning
CONFIG -= app_bundle

win32-msvc*:QMAKE_CXXFLAGS += /FS /wd4290 /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS

disable-installer:DEFINES *= AM_DISABLE_INSTALLER
systemd-sucks:DEFINES *= AM_SYSTEMD_SUCKS
headless:DEFINES *= AM_HEADLESS

!win32:exists($$SOURCE_DIR/.git):GIT_VERSION=$$system(cd "$$SOURCE_DIR" && git describe --tags --always 2>/dev/null)
isEmpty(GIT_VERSION):GIT_VERSION="unknown"
DEFINES *= AM_GIT_VERSION=\\\"$$GIT_VERSION\\\"


defineTest(CONFIG_VALUE) {
    !contains(CONFIG, "^$$1=.*"):return(false)
    value = $$find(CONFIG, "^$$1=.*")
    !isEmpty(value):value=$$section(value, =, 1, 1)
    $$2 = $$value
    export($$2)
    return(true)
}

!CONFIG_VALUE(install-prefix, INSTALL_PREFIX):INSTALL_PREFIX = /usr/local

CONFIG_VALUE(hardware-id, hw_id):DEFINES *= AM_HARDWARE_ID=\\\"$$hw_id\\\"
else:CONFIG_VALUE(hardware-id-from-file, hw_id_ff):DEFINES *= AM_HARDWARE_ID_FROM_FILE=\\\"$$hw_id_ff\\\"


defineReplace(fixLibraryPath) {
    libpath = $$1
    win32 {
        CONFIG(debug, debug|release) {
            libpath = $$join(libpath,,,/debug)
        } else {
            libpath = $$join(libpath,,,/release)
        }
    }
    return($$libpath)
}

defineTest(addStaticLibrary) {
    libpath = $$1
    libext = ".a"
    libprefix = "lib"
    srcdir = $$join(libpath,,$$join(PWD,,,'/'),)
    libname = $$basename(libpath)
    libdir = $$join(libpath,,$$join(OUT_PWD,,,'/'),)

    libdir = $$fixLibraryPath($$libdir)
    win32 {
        libext = ".lib"
        libprefix = ""
    }
    libdir=$$clean_path($$libdir)

    INCLUDEPATH *= $$srcdir
    CONFIG *= link_prl

    *-gcc*:LIBS += -Wl,--whole-archive
    LIBS += $$join(libdir,,-L,)
    LIBS += $$join(libname,,-l,)
    *-gcc*:LIBS += -Wl,--no-whole-archive

    PRE_TARGETDEPS *= $$join(libdir,,,$$join(libname,,"/$$libprefix",$$libext))

    export(INCLUDEPATH)
    export(LIBS)
    export(PRE_TARGETDEPS)
    export(CONFIG)
}

defineReplace(yesNo) {
   if ($$1):return("yes")
   else:return("no")
}

defineTest(printConfigLine) {
    !build_pass:return

    msg="$$1"
    val=$$2
    color=$$3
    width=$$4

    isEmpty(width):width = 30

    unix:system("tty -s") { # check if we are on unix and stdout is a tty
        equals(color, "auto") {
            yesmatch = $$find(val, "^yes")
            nomatch = $$find(val, "^no")
            automatch = $$find(val, "^auto")

            !isEmpty(yesmatch):color = "green"
            else:!isEmpty(nomatch):color = "red"
            else:!isEmpty(automatch):color = "yellow"
        }
        equals(color, "red"):         prolog=$$system(echo "\\\\033")[31;1m
        else:equals(color, "green"):  prolog=$$system(echo "\\\\033")[32;1m
        else:equals(color, "yellow"): prolog=$$system(echo "\\\\033")[33;1m
        else:equals(color, "orange"): prolog=$$system(echo "\\\\033")[33m
        else:equals(color, "white"):  prolog=$$system(echo "\\\\033")[37;1m
        epilog = $$system(echo "\\\\033")[0m
    }

    isEmpty(val) {
        log($$prolog$$msg$$epilog$$escape_expand(\\n))
        return()
    }

    # The tricky part: there are no arithmetic functions in qmake!
    # Start by createing an array of strings, where the string at [i] consists of i dots
    # We need it the other way around though, hence the reverse at the end (sadly you
    # cannot run a $$width..1 loop, although 30..1 does work).
    for(i, 1..$$width) {
        spacingEntry=""
        for (j, 1..$$i) { spacingEntry += "." }
        spacing += $$join(spacingEntry)
    }
    spacing = $$reverse(spacing)

    # convert a string into an array of characters, so we can get the length via size()
    msgArray = $$split(msg,)

    log("  $$msg $$member(spacing, $$size(msgArray)) $$prolog$$val$$epilog$$escape_expand(\\n)")
}

!win32:include(coverage.pri)
