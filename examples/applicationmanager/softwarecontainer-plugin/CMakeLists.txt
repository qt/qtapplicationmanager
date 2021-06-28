
cmake_minimum_required(VERSION 3.14)
project(softwarecontainer-plugin LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/softwarecontainer-plugin")

find_package(Qt6 COMPONENTS Core DBus AppManCommonPrivate AppManPluginInterfacesPrivate)

qt_add_plugin(softwarecontainer-plugin)
target_sources(softwarecontainer-plugin PRIVATE
    softwarecontainer.cpp softwarecontainer.h
)
set_target_properties(softwarecontainer-plugin PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
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
