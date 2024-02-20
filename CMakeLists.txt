
cmake_minimum_required(VERSION 3.16)

set(MIN_QT_VERSION 6.5.0)

include(.cmake.conf)
project(QtApplicationManager # special case
    VERSION "${QT_REPO_MODULE_VERSION}"
    DESCRIPTION "QtAplicationManager Tools and Libraries" # special case
    HOMEPAGE_URL "https://qt.io/"
    LANGUAGES CXX C
)

set(QT_NO_INTERNAL_COMPATIBILITY_FUNCTIONS TRUE)
add_compile_definitions(
    QT_STRICT_ITERATORS
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_ASCII
)

# populate the platform check variables
find_package(Qt6 ${PROJECT_VERSION} CONFIG REQUIRED COMPONENTS BuildInternals)

if(NOT (ANDROID OR IOS OR LINUX OR MACOS OR (WIN32 AND NOT WINRT)))
    if (QNX)
        message(NOTICE "QNX is not officially supported - continuing anyway ...")
    else()
        message(NOTICE "Skipping the build as building is only supported on Windows, macOS, Linux, Android and iOS.")
        return()
    endif()
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

if (${Qt6_VERSION} VERSION_LESS "${MIN_QT_VERSION}")
    message(FATAL_ERROR "Qt ${MIN_QT_VERSION} or newer is required for building version ${PROJECT_VERSION} of the QtApplicationManager")
endif()

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
    set(QT_BUILD_TESTS_BY_DEFAULT ON)
    add_compile_options(-fprofile-arcs -ftest-coverage -fprofile-update=atomic)
    add_link_options(-fprofile-arcs -fprofile-generate --coverage)
    add_compile_definitions(AM_COVERAGE)
    link_libraries(gcov)

    set(GCOV_EXCLUDE
        '/usr/*'
        '${QT_BUILD_INTERNALS_RELOCATABLE_INSTALL_PREFIX}/*'
        '${CMAKE_CURRENT_LIST_DIR}/tests/*'
        '${CMAKE_CURRENT_LIST_DIR}/examples/*'
        '${CMAKE_CURRENT_LIST_DIR}/src/3rdparty/*'
        '${CMAKE_CURRENT_LIST_DIR}/src/plugin-interfaces/*'
        'moc_*'
        '${CMAKE_CURRENT_BINARY_DIR}/*'
    )

    if(QT_SOURCE_TREE)
        string(REGEX REPLACE "/qtbase$" "" qtSrcDir ${QT_SOURCE_TREE})
        list(APPEND GCOV_EXCLUDE '${qtSrcDir}/*')
    endif()

    add_custom_target(check_coverage
        COMMAND sh -c 'which lcov >/dev/null || { echo "The lcov tool is not available." && exit 2 \; }'
        COMMAND sh -c 'which genhtml >/dev/null || { echo "The genhtml tool is not available." && exit 2 \; }'
        COMMAND find . \\\( -name \"*.gcov-info\" -o -name \"*.gcda\" \\\) -print0 | xargs -0 rm -f
        COMMAND lcov -keep-going -c -i -d . --rc branch_coverage=1 --rc geninfo_auto_base=1 -o "${CMAKE_CURRENT_BINARY_DIR}/base.gcov-info"
        COMMAND ctest --test-dir tests/auto --verbose
        COMMAND lcov -keep-going -c -d . --rc branch_coverage=1 --rc geninfo_auto_base=1 -o "${CMAKE_CURRENT_BINARY_DIR}/test.gcov-info"
        COMMAND lcov -keep-going --rc branch_coverage=1 -o "${CMAKE_CURRENT_BINARY_DIR}/temp.gcov-info" `find . -name \"*.gcov-info\" | xargs -n1 echo -a`
        COMMAND lcov -keep-going --rc branch_coverage=1 -o "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.gcov-info" -r temp.gcov-info ${GCOV_EXCLUDE}
        COMMAND rm -f base.gcov-info test.gcov-info temp.gcov-info
        COMMAND genhtml -o coverage-report -s -f -t "${PROJECT_NAME}" --legend --branch-coverage --rc branch_coverage=1 --rc geninfo_unexecuted_blocks=1 --demangle-cpp "c++filt" ${PROJECT_NAME}.gcov-info
        COMMAND echo \"\\n\\nCoverage info is available at file://`pwd`/coverage-report/index.html\\n\"

        DEPENDS
            qtapplicationmanager create-test-packages tests/auto/install
        USES_TERMINAL
    )
endif()

## SANITIZE ###########################################################

if (AM_SANITIZE)
    if (MSVC)
        set(ECM_ENABLE_SANITIZERS address)
    else()
        set(ECM_ENABLE_SANITIZERS address undefined)
    endif()
    include(3rdparty/extra-cmake-modules/modules/ECMEnableSanitizers)
endif()

## BUILD ##############################################################

qt_build_repo()

## DOCUMENTATION ######################################################

file(GLOB_RECURSE allDocFiles "doc/*.qdoc" "doc/*.png" "doc/*.qdocconf")

if(QT_BUILD_ONLINE_DOCS)
    set(DOC_CONF "doc/online/qtapplicationmanager.qdocconf")
else()
    set(DOC_CONF "doc/qtapplicationmanager.qdocconf")
endif()

add_custom_target(Documentation SOURCES
    doc/QtApplicationManagerDoc
    ${allDocFiles}
)

qt_internal_add_docs(Documentation
    ${DOC_CONF}
)

file(GLOB licenses "LICENSE.*[^~]")

add_custom_target("Other-Files" SOURCES
    .cmake.conf
    configure.cmake
    qt_cmdline.cmake
    dependencies.yaml
    sync.profile
    ${licenses}
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
