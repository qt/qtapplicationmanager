
if (NOT QT_CONFIGURE_RUNNING)
    include(QtAppManVersionDefines)
endif()

# all features are prefixed with "am" to prevent clashes with features in
# other Qt modules

#### Inputs

set(INPUT_libyaml "undefined" CACHE STRING "")
set_property(CACHE INPUT_libyaml PROPERTY STRINGS undefined qt system)

set(INPUT_libarchive "undefined" CACHE STRING "")
set_property(CACHE INPUT_libarchive PROPERTY STRINGS undefined qt system)


#### Libraries

if(TARGET WrapLibYaml::WrapLibYaml)
    qt_internal_disable_find_package_global_promotion(WrapLibYaml::WrapLibYaml)
endif()
qt_find_package(WrapLibYaml PROVIDED_TARGETS WrapLibYaml::WrapLibYaml MODULE_NAME appman_common QMAKE_LIB yaml)
qt_find_package(WrapLibArchive PROVIDED_TARGETS WrapLibArchive::WrapLibArchive MODULE_NAME appman_package QMAKE_LIB archive)


#### Tests


#### Features

qt_feature("am-system-libyaml" PRIVATE
    LABEL "Using system libyaml"
    CONDITION WrapLibYaml_FOUND
    ENABLE INPUT_libyaml STREQUAL 'system'
    DISABLE INPUT_libyaml STREQUAL 'qt'
)

qt_feature("am-system-libarchive" PRIVATE
    LABEL "Using system libarchive"
    CONDITION WrapLibArchive_FOUND
    ENABLE INPUT_libarchive STREQUAL 'system'
    DISABLE INPUT_libarchive STREQUAL 'qt'
)

qt_feature("am-multi-process" PUBLIC
    LABEL "Multi-process mode"
    CONDITION TARGET Qt::DBus AND TARGET Qt::WaylandCompositor AND LINUX AND QT_FEATURE_shared
    ENABLE INPUT_force_mode STREQUAL 'multi'
    DISABLE INPUT_force_mode STREQUAL 'single'
)

qt_feature("am-installer" PUBLIC
    LABEL "Enable the installer component"
    CONDITION QT_FEATURE_ssl AND NOT IOS
    ENABLE INPUT_installer STREQUAL 'yes'
    DISABLE INPUT_installer STREQUAL 'no'
)

qt_feature("am-external-dbus-interfaces" PRIVATE
    LABEL "Enable external DBus interfaces"
    CONDITION TARGET Qt::DBus
    ENABLE INPUT_external_dbus_interfaces STREQUAL 'yes'
    DISABLE INPUT_external_dbus_interfaces STREQUAL 'no'
)

qt_feature("am-package-server" PRIVATE
    LABEL "Build the package-server"
    CONDITION TARGET Qt::HttpServer AND QT_FEATURE_am_installer
    ENABLE INPUT_package_server STREQUAL 'yes'
    DISABLE INPUT_package_server STREQUAL 'no'
)

qt_feature("am-tools-only" PRIVATE
    LABEL "Tools only build"
    AUTODETECT OFF
    ENABLE INPUT_tools_only STREQUAL 'yes'
    DISABLE INPUT_tools_only STREQUAL 'no'
)

qt_feature("am-dltlogging" PRIVATE
    LABEL "Enable logging via DLT"
    CONDITION TARGET Qt::DltLogging
    ENABLE INPUT_dltlogging STREQUAL 'yes'
    DISABLE INPUT_dltlogging STREQUAL 'no'
)

qt_feature("am-libdbus" PRIVATE
    LABEL "Compile libdbus for appman-controller"
    CONDITION WIN32 OR MACOS
    EMIT_IF WIN32 OR MACOS
    ENABLE INPUT_libdbus STREQUAL 'yes'
    DISABLE INPUT_libdbus STREQUAL 'no'
)

qt_feature("am-libbacktrace" PRIVATE
    LABEL "Enable support for libbacktrace"
    CONDITION (LINUX OR MACOS) AND (CMAKE_BUILD_TYPE STREQUAL "Debug")
    EMIT_IF (LINUX OR MACOS)
    ENABLE INPUT_libbacktrace STREQUAL 'yes'
    DISABLE INPUT_libbacktrace STREQUAL 'no'
)

qt_feature("am-stackwalker" PRIVATE
    LABEL "Enable support for StackWalker"
    CONDITION WIN32 AND MSVC AND (CMAKE_BUILD_TYPE STREQUAL "Debug")
    EMIT_IF WIN32
    ENABLE INPUT_stackwalker STREQUAL 'yes'
    DISABLE INPUT_stackwalker STREQUAL 'no'
)

qt_feature("am-has-hardware-id" PRIVATE
    LABEL "Hardware-id for the installer"
    CONDITION ON
    DISABLE INPUT_hardware_id STREQUAL ''
)
qt_feature_definition("am-has-hardware-id" "QT_AM_HARDWARE_ID" VALUE "\"${INPUT_hardware_id}\"")

qt_feature("am-widgets-support" PUBLIC
    LABEL "Enable support for Qt widgets"
    CONDITION TARGET Qt::Widgets
    ENABLE INPUT_widgets_support STREQUAL 'yes'
    DISABLE INPUT_widgets_support STREQUAL 'no'
)

qt_configure_add_summary_section(NAME "Qt Application Manager")
qt_configure_add_summary_entry(ARGS "am-system-libyaml")
qt_configure_add_summary_entry(ARGS "am-system-libarchive")
qt_configure_add_summary_entry(ARGS "am-multi-process")
qt_configure_add_summary_entry(ARGS "am-installer")
if (QT_FEATURE_am_has_hardware_id)
    qt_configure_add_summary_entry(
        TYPE "message"
        ARGS "${QT_FEATURE_LABEL_am_has_hardware_id}"
        MESSAGE "yes (${INPUT_hardware_id})"
    )
else()
    qt_configure_add_summary_entry(ARGS "am-has-hardware-id")
endif()
qt_configure_add_summary_entry(ARGS "am-external-dbus-interfaces")
qt_configure_add_summary_entry(ARGS "am-widgets-support")
qt_configure_add_summary_entry(ARGS "am-tools-only")
qt_configure_add_summary_entry(ARGS "am-package-server")
qt_configure_add_summary_entry(ARGS "am-dltlogging")
qt_configure_add_summary_entry(ARGS "am-libdbus")
qt_configure_add_summary_entry(ARGS "am-libbacktrace")
qt_configure_add_summary_entry(ARGS "am-stackwalker")
qt_configure_end_summary_section() # end of "Qt ApplicationManger" section
