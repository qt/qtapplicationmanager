#### Inputs

set(INPUT_libyaml "undefined" CACHE STRING "")
set_property(CACHE INPUT_libyaml PROPERTY STRINGS undefined qt system)


#### Libraries

qt_find_package(WrapLibYaml PROVIDED_TARGETS WrapLibYaml::WrapLibYaml MODULE_NAME appman_common QMAKE_LIB yaml)


#### Tests


#### Features

qt_feature("system-libyaml" PRIVATE
    LABEL "Using system libyaml"
    CONDITION WrapLibYaml_FOUND
    ENABLE INPUT_libyaml STREQUAL 'system'
    DISABLE INPUT_libyaml STREQUAL 'qt'
)

qt_feature("multi-process" PUBLIC
    LABEL "Multi-process mode"
    CONDITION TARGET Qt::DBus AND TARGET Qt::WaylandCompositor AND LINUX
    ENABLE INPUT_force_mode STREQUAL 'multi'
    DISABLE INPUT_force_mode STREQUAL 'single'
)
qt_feature_definition("multi-process" "AM_MULTI_PROCESS")

qt_feature("installer" PRIVATE
    LABEL "Enable the installer component"
    CONDITION QT_FEATURE_ssl
    DISABLE INPUT_installer STREQUAL 'no'
)
qt_feature_definition("installer" "AM_DISABLE_INSTALLER" NEGATE)

qt_feature("external-dbus-interfaces" PRIVATE
    LABEL "Enable external DBus interfaces"
    CONDITION NOT INPUT_external_dbus_interfaces STREQUAL 'no' AND TARGET Qt::DBus
)
qt_feature_definition("external-dbus-interfaces" "AM_DISABLE_EXTERNAL_DBUS_INTERFACES" NEGATE)

qt_feature("tools-only" PRIVATE
    LABEL "Tools only build"
    CONDITION INPUT_tools_only STREQUAL 'yes'
)

qt_feature("libbacktrace" PRIVATE
    LABEL "Enable support for libbacktrace"
    CONDITION LINUX AND ( ( INPUT_libbacktrace STREQUAL 'yes' ) OR ( ( CMAKE_BUILD_TYPE STREQUAL "Debug" ) AND ( NOT INPUT_libbacktrace STREQUAL 'no' ) ) )
    EMIT_IF LINUX
)
qt_feature_definition("libbacktrace" "AM_USE_LIBBACKTRACE")

qt_feature("stackwalker" PRIVATE
    LABEL "Enable support for StackWalker"
    CONDITION WIN32 AND ( ( INPUT_stackwalker STREQUAL 'yes' ) OR ( ( CMAKE_BUILD_TYPE STREQUAL "Debug") AND (NOT INPUT_stackwalker STREQUAL 'no' ) ) )
    EMIT_IF WIN32
)
qt_feature_definition("stackwalker" "AM_USE_STACKWALKER")

qt_configure_add_summary_section(NAME "Qt Application Manager")
qt_configure_add_summary_entry(ARGS "system-libyaml")
qt_configure_add_summary_entry(ARGS "multi-process")
qt_configure_add_summary_entry(ARGS "installer")
qt_configure_add_summary_entry(ARGS "external-dbus-interfaces")
qt_configure_add_summary_entry(ARGS "tools-only")
qt_configure_add_summary_entry(ARGS "libbacktrace")
qt_configure_add_summary_entry(ARGS "stackwalker")
qt_configure_end_summary_section() # end of "Qt ApplicationManger" section

qt_extra_definition("AM_VERSION" "\"${PROJECT_VERSION}\"" PUBLIC)
