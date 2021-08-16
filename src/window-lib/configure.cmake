#### Inputs

#### Libraries

qt_find_package(X11)

#### Tests

#### Features

qt_feature("am-touch-emulation" PRIVATE
    LABEL "Touch emulation support on X11"
    CONDITION X11_Xi_FOUND AND LINUX
    EMIT_IF LINUX
)
qt_feature_definition("am-touch-emulation" "AM_ENABLE_TOUCH_EMULATION")

qt_feature("am-widgets-support" PRIVATE PUBLIC
    LABEL "Enable support for Qt widgets"
    CONDITION INPUT_widgets_support STREQUAL 'yes' AND TARGET Qt::Widgets
)
qt_feature_definition("am-widgets-support" "AM_WIDGETS_SUPPORT")

qt_configure_add_summary_section(NAME "Qt Application Manager [Window module]")
qt_configure_add_summary_entry(ARGS "am-touch-emulation")
qt_configure_add_summary_entry(ARGS "am-widgets-support")
qt_configure_end_summary_section() # end of "Qt ApplicationManger" section

qt_extra_definition("AM_VERSION" "\"${PROJECT_VERSION}\"" PUBLIC)
