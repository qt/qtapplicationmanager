cmake_minimum_required(VERSION 3.16)
project(softwarecontainer-plugin LANGUAGES CXX)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/softwarecontainer-plugin")


find_package(Qt6 COMPONENTS Core DBus AppManCommonPrivate AppManPluginInterfacesPrivate)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_plugin(softwarecontainer-plugin
    softwarecontainer.h
    softwarecontainer.cpp
)

target_link_libraries(softwarecontainer-plugin PUBLIC
    Qt::Core
    Qt::DBus
    Qt::AppManCommonPrivate
    Qt::AppManPluginInterfacesPrivate
)

install(TARGETS softwarecontainer-plugin
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)
