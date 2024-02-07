
if(NOT QT_FEATURE_am_system_libarchive AND QT_FEATURE_am_installer)
    add_subdirectory(libarchive)
endif()

if(NOT QT_FEATURE_am_system_libyaml)
    add_subdirectory(libyaml)
endif()

if(QT_FEATURE_am_libdbus)
    add_subdirectory(libdbus)
endif()

if(QT_FEATURE_am_libbacktrace)
    add_subdirectory(libbacktrace)
endif()

if(QT_FEATURE_am_stackwalker)
    add_subdirectory(stackwalker)
endif()
