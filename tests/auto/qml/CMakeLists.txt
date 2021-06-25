# Generated from qml.pro.

# Setup the test when called as a completely standalone project.
if(TARGET Qt6::Core)
    # Tests are built as part of the qtsensors build tree.
    # Setup paths so that the Qt packages are found.
    qt_internal_set_up_build_dir_package_paths()
endif()

add_subdirectory(simple)
add_subdirectory(windowmanager)
add_subdirectory(windowmapping)
add_subdirectory(windowitem)
add_subdirectory(windowitem2)
add_subdirectory(installer)
add_subdirectory(quicklaunch)
add_subdirectory(intents)
add_subdirectory(configs)
add_subdirectory(lifecycle)
add_subdirectory(resources)
if(multi-process)
    add_subdirectory(crash/apps/tld.test.crash/terminator2)
    add_subdirectory(crash)
    add_subdirectory(processtitle)
endif()
