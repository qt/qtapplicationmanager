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

        # The generated _adaptor.cpp files are not usable as is, so we implement
        # that part ourselves. We have to make sure to not compile the generated
        # ones though.

        # reconstruct the .cpp name (see QtDbusHelpers.cmake)
        get_filename_component(file_name "${adaptor}" NAME_WLE)
        get_filename_component(file_ext "${file_name}" LAST_EXT)
        if("x${file_ext}" STREQUAL "x")
        else()
            string(SUBSTRING "${file_ext}" 1 -1 file_name) # cut of leading '.'
        endif()
        string(TOLOWER "${file_name}" file_name)
        set(file_name "${file_name}_adaptor.cpp")

        # remove the .cpp file from SOURCES
        get_target_property(srcs ${target} SOURCES)
        list(REMOVE_ITEM srcs "${file_name}")
        set_target_properties(${target} PROPERTIES SOURCES "${srcs}")

    endforeach()
endfunction()
