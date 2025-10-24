
qt_get_tool_target_name(target_name appman-package-server)
qt_internal_add_tool(${target_name}
    USER_FACING
    EXCEPTIONS
    TOOLS_TARGET AppManSystemUIPrivate
    SOURCES
        package-server.cpp package-server.h
        psconfiguration.h psconfiguration.cpp
        pspackages.h pspackages.cpp pspackages_p.h
        pshttpinterface.h pshttpinterface.cpp pshttpinterface_p.h
    LIBRARIES
        Qt::Network
        Qt::AppManCommonPrivate
        Qt::AppManPackagePrivate
        Qt::HttpServer
    DEFINES
        AM_COMPILING_APPMAN
)
qt_internal_return_unless_building_tools()
