
cmake_minimum_required(VERSION 3.14)
project(startup-plugin LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/startup-plugin")

find_package(Qt6 COMPONENTS Core AppManPluginInterfacesPrivate)

qt_add_plugin(startup-plugin)
target_sources(startup-plugin PRIVATE
    startup-plugin.cpp startup-plugin.h
)
set_target_properties(startup-plugin PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
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
