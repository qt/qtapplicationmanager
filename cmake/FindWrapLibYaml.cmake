# We can't create the same interface imported target multiple times, CMake will complain if we do
# that. This can happen if the find_package call is done in multiple different subdirectories.
if(TARGET WrapLibYaml::WrapLibYaml)
    set(WrapLibYaml_FOUND TRUE)
    return()
endif()

find_package(PkgConfig)
pkg_check_modules(pc_libyaml yaml-0.1>=0.2.2 IMPORTED_TARGET)


if (NOT pc_libyaml_FOUND)
    set(WrapLibYaml_FOUND FALSE)
    return()
endif()

add_library(WrapLibYaml::WrapLibYaml INTERFACE IMPORTED)
target_link_libraries(WrapLibYaml::WrapLibYaml INTERFACE ${pc_libyaml_LIBRARIES})
set(WrapLibYaml_FOUND TRUE)
