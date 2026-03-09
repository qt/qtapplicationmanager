cmake_minimum_required(VERSION 3.16)
project(custom-appman LANGUAGES CXX)

find_package(Qt6 COMPONENTS AppManSystemUIPrivate)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(custom-appman
    custom-appman.cpp
)

if (QT_FEATURE_static)
    # This define flags us as an "appman" and allows us to link against the AppMan's private libraries
    target_compile_definitions(custom-appman PRIVATE AM_COMPILING_APPMAN)
endif()

target_compile_definitions(custom-appman PRIVATE
    QT_MESSAGELOGCONTEXT
)

target_link_libraries(custom-appman PRIVATE
    Qt::AppManSystemUIPrivate
)

install(TARGETS custom-appman
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
