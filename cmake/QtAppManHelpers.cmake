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

        if (${Qt6_VERSION} VERSION_LESS "6.4.0")
            set(file_name "${file_name}_adaptor.cpp")
        else()
            set(file_name "${CMAKE_CURRENT_BINARY_DIR}/${file_name}_adaptor.cpp")
        endif()

        # remove the .cpp file from SOURCES
        get_target_property(srcs ${target} SOURCES)
        list(REMOVE_ITEM srcs "${file_name}")
        set_target_properties(${target} PROPERTIES SOURCES "${srcs}")

    endforeach()
endfunction()

# Determines if QNX version is at least 8.0. If so, sets TEST_qnx_version_8 to true.
# __QNX__ is defined by qcc; its value is the API release version with the decimal points removed
# (e.g. 801 == 8.0.1). It supersedes the deprecated _NTO_VERSION from <sys/neutrino.h>.
macro(qt_am_internal_check_qnx_version)
    qt_config_compile_test(qnx_version_8
        LABEL "QNX version >= 8.0"
        CODE "
#if !defined(__QNX__) || __QNX__ < 800
#  error __QNX__ is less than 800
#endif
int main(int, char **) { return 0; }
")
    qt_run_config_compile_test(qnx_version_8)
endmacro()
