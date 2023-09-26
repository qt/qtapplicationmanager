
add_custom_target("DBus-Interface-XMLs" SOURCES
    io.qt.packagemanager.xml
    io.qt.applicationmanager.applicationinterface.xml
    io.qt.applicationmanager.runtimeinterface.xml
    io.qt.applicationmanager.intentinterface.xml
    io.qt.applicationmanager.xml
    io.qt.windowmanager.xml
    org.freedesktop.notifications.xml
)

# quick way to re-gerenate the dbus interface xml files: the .regen files have to be merged
# with the existing xml files though, as there are manual edits necessary.
if (TARGET Qt::DBus)
    qt_generate_dbus_interface(
        ${CMAKE_SOURCE_DIR}/src/manager-lib/applicationmanager.h
        ${CMAKE_CURRENT_BINARY_DIR}/io.qt.applicationmanager.xml.regen
        OPTIONS -a -t "QtAM::Am::RunState=u" -t "QtAM::UnixFdMap=a{sh}"
    )
    qt_generate_dbus_interface(
        ${CMAKE_SOURCE_DIR}/src/manager-lib/packagemanager.h
        ${CMAKE_CURRENT_BINARY_DIR}/io.qt.packagemanager.xml.regen
        OPTIONS -a -t "QtAM::AsynchronousTask::TaskState=s"
    )
    qt_generate_dbus_interface(
        ${CMAKE_SOURCE_DIR}/src/window-lib/windowmanager.h
        ${CMAKE_CURRENT_BINARY_DIR}/io.qt.windowmanager.xml.regen
        OPTIONS -a
    )
    add_custom_target(dbus_xml_regen DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/io.qt.applicationmanager.xml.regen
        ${CMAKE_CURRENT_BINARY_DIR}/io.qt.packagemanager.xml.regen
        ${CMAKE_CURRENT_BINARY_DIR}/io.qt.windowmanager.xml.regen
    )
endif()
