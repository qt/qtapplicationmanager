find_package(Qt6 COMPONENTS AppManApplicationMainPrivate)

qt_add_executable(widgets main.cpp)

# This define flags us as a "launcher"
target_compile_definitions(widgets PRIVATE AM_COMPILING_LAUNCHER)

target_link_libraries(widgets
    PRIVATE
        Qt::Widgets
        Qt::AppManApplicationMainPrivate
)

qt6_am_create_builtin_package(Widgets
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    INSTALL_DIRECTORY "${INSTALL_EXAMPLEDIR}/apps/Widgets"
    FILES
        info.yaml
        icon.png
    DEPENDENCIES
        widgets
)
