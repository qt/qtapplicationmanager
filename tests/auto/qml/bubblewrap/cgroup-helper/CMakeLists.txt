set(CMAKE_AUTOMOC TRUE)

qt_add_qml_module(cgroupTestHelperModule
    URI "CGroupTestHelper"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/CGroupTestHelper"
    SOURCES cgrouptesthelper.h cgrouptesthelper.cpp
)

target_link_libraries(cgroupTestHelperModule PRIVATE
    Qt::AppManCommonPrivate
)
