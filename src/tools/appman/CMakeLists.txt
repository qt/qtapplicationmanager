# Generated from appman.pro.

#####################################################################
## appman Tool:
#####################################################################

qt_get_tool_target_name(target_name appman)
qt_internal_add_tool(${target_name}
    USER_FACING
    EXCEPTIONS
    TOOLS_TARGET AppManMainPrivate
    SOURCES
        appman.cpp
    PUBLIC_LIBRARIES
        Qt::AppManMainPrivate
)

#### Keys ignored in scope 1:.:.:appman.pro:<TRUE>:
# GIT_VERSION = "$$cat($$SOURCE_DIR/.tag, lines)"
# TEMPLATE = "app"

## Scopes:
#####################################################################

#### Keys ignored in scope 4:.:.:appman.pro:UNIX AND EXISTS _ss_SOURCE_DIR/.git:
# GIT_VERSION = "$$system(cd "$$SOURCE_DIR" && git describe --tags --always --dirty 2>/dev/null)"

#### Keys ignored in scope 5:.:.:appman.pro:else:
# GIT_VERSION = "unknown"

#### Keys ignored in scope 6:.:.:appman.pro:ANDROID:
# INSTALLS = <EMPTY>
