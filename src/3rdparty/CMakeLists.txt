if(NOT QT_FEATURE_system_libarchive)
    add_subdirectory(libarchive)
endif()

if(NOT QT_FEATURE_system_libyaml)
    add_subdirectory(libyaml)
endif()

if(QT_FEATURE_libbacktrace)
    add_subdirectory(libbacktrace)
endif()

if(QT_FEATURE_stackwalker)
    add_subdirectory(stackwalker)
endif()
