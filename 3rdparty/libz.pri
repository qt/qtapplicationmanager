# zlib dependency satisfied by bundled 3rd party zlib or system zlib
contains(QT_CONFIG, zlib) {
    CONFIG *= qt
    QT *= core

    load(qt_build_paths)
    git_build: \
        INCLUDEPATH += $$[QT_INSTALL_HEADERS/get]/QtZlib
    else: \
        INCLUDEPATH += $$[QT_INSTALL_HEADERS/src]/QtZlib
} else {
    unix|mingw: LIBS_PRIVATE += -lz
    else:       LIBS += zdll.lib
}
