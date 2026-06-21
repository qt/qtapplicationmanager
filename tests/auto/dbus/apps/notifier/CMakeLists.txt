set(CMAKE_AUTOMOC TRUE)

qt_add_qml_module(
    notifyHelperModule
    URI "NotifyHelper"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/NotifyHelper"
    SOURCES notifyhelper.h notifyhelper.cpp
)

target_link_libraries(notifyHelperModule PRIVATE Qt::DBus)
