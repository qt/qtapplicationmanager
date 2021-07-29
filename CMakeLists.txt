
cmake_minimum_required(VERSION 3.16)

include(.cmake.conf)
project(QtApplicationManager # special case
    VERSION "${QT_REPO_MODULE_VERSION}"
    DESCRIPTION "QtAplicationManager Tools and Libraries" # special case
    HOMEPAGE_URL "https://qt.io/"
    LANGUAGES CXX C
)

set(QT_NO_INTERNAL_COMPATIBILITY_FUNCTIONS TRUE)

# populate the platform check variables
find_package(Qt6 ${PROJECT_VERSION} CONFIG REQUIRED COMPONENTS BuildInternals)

if(NOT (ANDROID OR IOS OR LINUX OR MACOS OR (WIN32 AND NOT WINRT)))
    message(NOTICE "Skipping the build as building is only supported on Windows, macOS, Linux, Android and iOS.")
    return()
endif()

# find the all modules (Gui isn't really optional, but we have to deal with a no-gui CI configuration)
find_package(Qt6 ${PROJECT_VERSION} CONFIG REQUIRED COMPONENTS Core Concurrent)
find_package(Qt6 ${PROJECT_VERSION} QUIET CONFIG OPTIONAL_COMPONENTS
    Gui
    Qml
    Quick
    QuickTest
    DBus
    WaylandClient
    WaylandCompositor
    QuickTest
    Widgets
    ZlibPrivate
    DltLogging
    WaylandGlobalPrivate # remove after QTBUG-95388 is fixed
)

include(src/main-lib/Qt6AppManMainPrivateMacros.cmake)

if(NOT TARGET Qt::Gui OR NOT TARGET Qt::Quick)
    message(NOTICE "Skipping the build as QtGui and/or QtQuick are not available.")
    return()
endif()

## COVERAGE ###########################################################

if(AM_COVERAGE)
    if(NOT (LINUX AND (GCC OR CLANG)))
        message(FATAL_ERROR "Coverage builds are only supported on Linux, using GCC or Clang.")
        return()
    endif()

    set(QT_BUILD_TESTS ON)
    set(GCC_COVERAGE_COMPILE_FLAGS "-fprofile-arcs -ftest-coverage")
    set(GCC_COVERAGE_LINK_FLAGS    "-fprofile-arcs -fprofile-generate -lgcov --coverage")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")

    set(GCOV_EXCLUDE
        '/usr/*'
        '${QT_BUILD_INTERNALS_RELOCATABLE_INSTALL_PREFIX}/*'
        '${CMAKE_CURRENT_LIST_DIR}/tests/*'
        '${CMAKE_CURRENT_LIST_DIR}/examples/*'
        '${CMAKE_CURRENT_LIST_DIR}/src/3rdparty/*'
        'moc_*'
        '${CMAKE_CURRENT_BINARY_DIR}/*'
    )

    if(QT_SOURCE_TREE)
        string(REGEX REPLACE "/qtbase$" "" qtSrcDir ${QT_SOURCE_TREE})
        list(APPEND GCOV_EXCLUDE '${qtSrcDir}/*')
    endif()

    add_custom_target(check_coverage
        find . -name \"*.gcov-info\" -print0 | xargs -0 rm -f &&
        lcov -c -i -d . --rc lcov_branch_coverage=1 --rc geninfo_auto_base=1 -o "${CMAKE_CURRENT_BINARY_DIR}/base.gcov-info" >/dev/null 2>&1 &&
        ctest --test-dir tests/auto --verbose &&
        lcov -c -d . --rc lcov_branch_coverage=1 --rc geninfo_auto_base=1 -o "${CMAKE_CURRENT_BINARY_DIR}/test.gcov-info" >/dev/null &&
        lcov --rc lcov_branch_coverage=1 -o "${CMAKE_CURRENT_BINARY_DIR}/temp.gcov-info" `find . -name \"*.gcov-info\" | xargs -n1 echo -a` >/dev/null &&
        lcov --rc lcov_branch_coverage=1 -o "${CMAKE_CURRENT_BINARY_DIR}/qtapplicationmanager.gcov-info" -r temp.gcov-info ${GCOV_EXCLUDE} >/dev/null &&
        rm -f base.gcov-info test.gcov-info temp.gcov-info &&
        genhtml -o branch-coverage -s -f --legend --branch-coverage --rc lcov_branch_coverage=1 --demangle-cpp qtapplicationmanager.gcov-info >/dev/null &&
        echo \"\\n\\nCoverage info is available at file://`pwd`/branch-coverage/index.html\\n\"

        DEPENDS
            tests/auto/install
        USES_TERMINAL
    )
endif()

## BUILD ##############################################################

qt_build_repo()

## DOCUMENTATION ######################################################

file(GLOB_RECURSE allDocFiles "doc/*.qdoc" "doc/*.png" "doc/*.qdocconf")

add_custom_target(Documentation SOURCES
    doc/QtApplicationManagerDoc
    ${allDocFiles}
)

qt_internal_add_docs(Documentation
    doc/qtapplicationmanager.qdocconf
)

file(GLOB licenses "LICENSE.*[^~]")
file(GLOB templateYamls "template-opt/am/*.yaml")

add_custom_target("Other-Files" SOURCES
    .cmake.conf
    configure.cmake
    qt_cmdline.cmake
    dependencies.yaml
    sync.profile
    ${licenses}
    ${templateYamls}
    util/bash/appman-prompt
    util/bash/README
)

## QMAKE FEATURES #####################################################

if(NOT QT_BUILD_STANDALONE_TESTS)
    # Copy mkspecs for users preferring qmake builds
    set(mkspecs_install_dir "${INSTALL_MKSPECSDIR}")
    qt_path_join(mkspecs_install_dir ${QT_INSTALL_DIR} ${mkspecs_install_dir})

    file(GLOB QMAKE_FEATURES
        "qmake-features/*.prf"
    )

    qt_copy_or_install(
        FILES "${QMAKE_FEATURES}"
        DESTINATION "${mkspecs_install_dir}/features"
    )
endif()
