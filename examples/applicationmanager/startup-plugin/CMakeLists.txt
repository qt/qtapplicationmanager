cmake_minimum_required(VERSION 3.16)
project(startup-plugin LANGUAGES CXX)

find_package(Qt6 COMPONENTS Core AppManPluginInterfacesPrivate)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_plugin(startup-plugin
    startup-plugin.h
    startup-plugin.cpp
    CLASS_NAME startup_plugin
)

target_link_libraries(startup-plugin PUBLIC
    Qt::Core
    Qt::AppManPluginInterfacesPrivate
)

install(TARGETS startup-plugin
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
