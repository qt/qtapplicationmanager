# We can't create the same interface imported target multiple times, CMake will complain if we do
# that. This can happen if the find_package call is done in multiple different subdirectories.
if(TARGET WrapLibSystemd::WrapLibSystemd)
    set(WrapLibSystemd_FOUND TRUE)
    return()
endif()

find_package(PkgConfig)
pkg_check_modules(pc_libsystemd libsystemd>=183 IMPORTED_TARGET)


if (NOT pc_libsystemd_FOUND)
    set(WrapLibSystemd_FOUND FALSE)
    return()
endif()

add_library(WrapLibSystemd::WrapLibSystemd INTERFACE IMPORTED)
target_link_libraries(WrapLibSystemd::WrapLibSystemd INTERFACE ${pc_libsystemd_LIBRARIES})
set(WrapLibSystemd_FOUND TRUE)
