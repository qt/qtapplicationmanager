find_package(Qt6 QUIET CONFIG OPTIONAL_COMPONENTS WaylandClient)

if(TARGET Qt::WaylandClient AND TARGET Qt::Widgets)
    add_subdirectory(widgets)
endif()
