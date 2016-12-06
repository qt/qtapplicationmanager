# zlib dependency satisfied by bundled 3rd party zlib or system zlib
lessThan(QT_MINOR_VERSION, 8) {
    # this is the "old way" used by Qt <= 5.7

    contains(QT_CONFIG, system-zlib) {
        unix|mingw: LIBS_PRIVATE += -lz
        else:       LIBS += zdll.lib
    } else {
        CONFIG *= qt
        QT *= core

        load(qt_build_paths)
        git_build: {
            INCLUDEPATH += $$[QT_INSTALL_HEADERS/get]/QtZlib
        } else {
            INCLUDEPATH += $$[QT_INSTALL_HEADERS/src]/QtZlib
        }
    }
} else {
    # the "new way", starting with Qt 5.8

    CONFIG *= qt

    qtConfig(system-zlib) {
        QMAKE_USE_PRIVATE *= zlib
    } else {
        QT_PRIVATE *= core zlib-private
    }
}
