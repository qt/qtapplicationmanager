cmake_minimum_required(VERSION 3.16)
project(startup-plugin LANGUAGES CXX)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/startup-plugin")


find_package(Qt6 COMPONENTS Core AppManPluginInterfacesPrivate)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_plugin(startup-plugin
    startup-plugin.h
    startup-plugin.cpp
)

target_link_libraries(startup-plugin PUBLIC
    Qt::Core
    Qt::AppManPluginInterfacesPrivate
)

install(TARGETS startup-plugin
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)
