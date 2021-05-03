# Generated from testrunner.pro.

#####################################################################
## appman-qmltestrunner Tool:
#####################################################################

qt_get_tool_target_name(target_name appman-qmltestrunner)
qt_internal_add_tool(${target_name}
    EXCEPTIONS
    TOOLS_TARGET AppManMainPrivate
    SOURCES
        ../appman/appman.cpp
        amtest.cpp amtest.h
        testrunner.cpp testrunner.h testrunner_p.h
    DEFINES
        AM_TESTRUNNER
    PUBLIC_LIBRARIES
        Qt::QuickTest
        Qt::QuickTestPrivate
        Qt::TestPrivate
        Qt::AppManMainPrivate
)

## Scopes:
#####################################################################

#### Keys ignored in scope 2:.:.:testrunner.pro:ANDROID:
# INSTALLS = <EMPTY>

#### Keys ignored in scope 6:.:../appman:../appman/appman.pro:UNIX AND EXISTS _ss_SOURCE_DIR/.git:
# GIT_VERSION = "$$system(cd "$$SOURCE_DIR" && git describe --tags --always --dirty 2>/dev/null)"

#### Keys ignored in scope 7:.:../appman:../appman/appman.pro:else:
# GIT_VERSION = "unknown"
