#### Inputs

set(INPUT_libarchive "undefined" CACHE STRING "")
set_property(CACHE INPUT_libarchive PROPERTY STRINGS undefined qt system)

#### Libraries

qt_find_package(WrapLibArchive PROVIDED_TARGETS WrapLibArchive::WrapLibArchive MODULE_NAME appman_package QMAKE_LIB archive)

#### Tests

#### Features

qt_feature("system-libarchive" PRIVATE
    LABEL "Using system libarchive"
    CONDITION WrapLibArchive_FOUND
    ENABLE INPUT_libarchive STREQUAL 'system'
    DISABLE INPUT_libarchive STREQUAL 'qt'
)

qt_configure_add_summary_section(NAME "Qt Application Manager [Packaging module]")
qt_configure_add_summary_entry(ARGS "system-libarchive")
qt_configure_end_summary_section() # end of "Qt ApplicationManger" section
