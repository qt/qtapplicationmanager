
qt_internal_add_app(appman-launcher-qml
    EXCEPTIONS
    SOURCES
        launcher-qml.cpp launcher-qml_p.h
    LIBRARIES
        Qt::CorePrivate
        Qt::DBus
        Qt::Gui
        Qt::GuiPrivate
        Qt::Qml
        Qt::Quick
        Qt::QuickPrivate
        Qt::AppManApplicationPrivate
        Qt::AppManCommonPrivate
        Qt::AppManSharedMainPrivate
        Qt::AppManApplicationMainPrivate
        Qt::AppManPluginInterfacesPrivate
    DEFINES
        AM_COMPILING_LAUNCHER
)
