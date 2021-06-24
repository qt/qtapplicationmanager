# Generated from application-manager.pro.

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
)

if(NOT TARGET Qt::Gui OR NOT TARGET Qt::Quick)
    message(NOTICE "Skipping the build as QtGui and/or QtQuick are not available.")
    return()
endif()

qt_build_repo()

add_custom_target(QtApplicationManagerDoc)

qt_internal_add_docs(QtApplicationManagerDoc
    doc/qtapplicationmanager.qdocconf
)
