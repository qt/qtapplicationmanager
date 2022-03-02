#### Inputs

#### Libraries

qt_find_package(X11)

#### Tests

#### Features

qt_feature("am-widgets-support" PRIVATE PUBLIC
    LABEL "Enable support for Qt widgets"
    CONDITION TARGET Qt::Widgets
    ENABLE INPUT_widgets_support STREQUAL 'yes'
    DISABLE INPUT_widgets_support STREQUAL 'no'
)
qt_feature_definition("am-widgets-support" "AM_WIDGETS_SUPPORT")

qt_configure_add_summary_section(NAME "Qt Application Manager [Window module]")
qt_configure_add_summary_entry(ARGS "am-widgets-support")
qt_configure_end_summary_section() # end of "Qt ApplicationManger" section

qt_extra_definition("AM_VERSION" "\"${PROJECT_VERSION}\"" PUBLIC)
