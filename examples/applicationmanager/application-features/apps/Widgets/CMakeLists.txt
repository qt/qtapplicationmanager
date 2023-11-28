cmake_minimum_required(VERSION 3.16)
project(widgets LANGUAGES CXX)

find_package(Qt6 COMPONENTS Widgets)
find_package(Qt6 COMPONENTS AppManApplicationMainPrivate)

qt_standard_project_setup()

qt_add_executable(widgets main.cpp)

# This define flags us as a "launcher" and allows us to link against the AppMan's private libraries
target_compile_definitions(widgets PRIVATE AM_COMPILING_LAUNCHER)

set_target_properties(
    widgets
    PROPERTIES
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE TRUE
    RUNTIME_OUTPUT_DIRECTORY .
)

target_link_libraries(widgets PUBLIC Qt::Widgets Qt::AppManApplicationMainPrivate)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
    set(INSTALL_EXAMPLESDIR "examples")
else()
    set_target_properties(widgets PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

set(INSTALL_DIR "${INSTALL_EXAMPLESDIR}/applicationmanager/application-features/apps/Widgets")

install(
    TARGETS widgets
    LIBRARY DESTINATION "${INSTALL_DIR}"
    RUNTIME DESTINATION "${INSTALL_DIR}"
    BUNDLE DESTINATION "${INSTALL_DIR}"
)
