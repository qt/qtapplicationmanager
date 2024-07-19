cmake_minimum_required(VERSION 3.16)
project(custom-appman LANGUAGES CXX)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/custom-appman")

find_package(Qt6 COMPONENTS AppManMainPrivate)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(custom-appman WIN32
    custom-appman.cpp
)

# This define flags us as an "appman" and allows us to link against the AppMan's private libraries
target_compile_definitions(custom-appman PRIVATE AM_COMPILING_APPMAN)

target_compile_definitions(custom-appman PRIVATE
    QT_MESSAGELOGCONTEXT
)

target_link_libraries(custom-appman PRIVATE
    Qt::AppManMainPrivate
)

install(TARGETS custom-appman
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)
