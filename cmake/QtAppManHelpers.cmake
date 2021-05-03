# This function is a hack to make generating DBus adaptors without custom cpp files possible
function(qtam_internal_add_dbus_adaptor target)
    if (NOT TARGET "${target}")
        message(FATAL_ERROR "Trying to extend non-existing target \"${target}\".")
    endif()

    cmake_parse_arguments(arg "" "" "DBUS_ADAPTOR_SOURCES;DBUS_ADAPTOR_FLAGS" ${ARGN})

    foreach(adaptor ${arg_DBUS_ADAPTOR_SOURCES})
        qt_internal_extend_target(${target}
            DBUS_ADAPTOR_SOURCES ${adaptor}
            DBUS_ADAPTOR_FLAGS ${arg_DBUS_ADAPTOR_FLAGS}
        )

        # hack to remove the .cpp file, which we implement ourselves
        get_target_property(srcs ${target} SOURCES)
        list(REMOVE_AT srcs -1)
        set_target_properties(${target} PROPERTIES SOURCES "${srcs}")

    endforeach()
endfunction()
