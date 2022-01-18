find_package(Qt6 COMPONENTS WaylandClient)

if(TARGET Qt::WaylandClient AND TARGET Qt::Widgets)
    add_subdirectory(widgets)
endif()
