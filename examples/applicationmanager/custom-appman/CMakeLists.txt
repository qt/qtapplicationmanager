
cmake_minimum_required(VERSION 3.16)
project(custom-appman LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED INSTALL_EXAMPLESDIR)
  set(INSTALL_EXAMPLESDIR "examples")
endif()

set(INSTALL_EXAMPLEDIR "${INSTALL_EXAMPLESDIR}/applicationmanager/custom-appman")

find_package(Qt6 COMPONENTS AppManMainPrivate)


qt_add_executable(custom-appman
    custom-appman.cpp
)
set_target_properties(custom-appman PROPERTIES
    WIN32_EXECUTABLE FALSE
    MACOSX_BUNDLE FALSE
)
target_compile_definitions(custom-appman PUBLIC
    QT_MESSAGELOGCONTEXT
)

target_link_libraries(custom-appman PUBLIC
    Qt::AppManMainPrivate
)

install(TARGETS custom-appman
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)
