
if(QT_BUILD_STANDALONE_TESTS)
    # Add qt_find_package calls for extra dependencies that need to be found when building
    # the standalone tests here.
    find_package(X11)
    find_package(ZLIB)
    find_package(Iconv)
    # Needed to get access to the AM's feature definitions, and some tests link against
    # AppManApplicationPrivate.
    qt_find_package(Qt6 COMPONENTS AppManApplicationPrivate AppManCommonPrivate)
endif()

add_subdirectory(data)

qt_build_tests()
