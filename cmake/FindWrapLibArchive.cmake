# We can't create the same interface imported target multiple times, CMake will complain if we do
# that. This can happen if the find_package call is done in multiple different subdirectories.
if(TARGET WrapLibArchive::WrapLibArchive)
    set(WrapLibArchive_FOUND TRUE)
    return()
endif()

find_package(LibArchive 3.4)

if (NOT LibArchive_FOUND)
    set(WrapLibArchive_FOUND FALSE)
    return()
endif()

add_library(WrapLibArchive::WrapLibArchive INTERFACE IMPORTED)
target_link_libraries(WrapLibArchive::WrapLibArchive INTERFACE ${LibArchive_LIBRARIES})
target_include_directories(WrapLibArchive::WrapLibArchive INTERFACE ${LibArchive_INCLUDE_DIRS})
set(WrapLibArchive_FOUND TRUE)
