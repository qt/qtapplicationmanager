
add_subdirectory(data)

if(QT_BUILD_STANDALONE_TESTS)
    # Add qt_find_package calls for extra dependencies that need to be found when building
    # the standalone tests here.
    find_package(X11)
    find_package(ZLIB)
    find_package(Iconv)
endif()
qt_build_tests()
