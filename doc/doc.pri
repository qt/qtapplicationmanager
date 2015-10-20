
exists($$[QT_INSTALL_BINS]/qdoc):exists($$[QT_INSTALL_BINS]/qhelpgenerator) {
    check_qdoc = "qdoc/qhelpgenerator in $$[QT_INSTALL_BINS]"
    QDOC = $$[QT_INSTALL_BINS]/qdoc
    QHELPGENERATOR = $$[QT_INSTALL_BINS]/qhelpgenerator
} else {
    check_qdoc = "qdoc/qhelpgenerator in PATH"
    QDOC = qdoc
    QHELPGENERATOR = qhelpgenerator
}

printConfigLine("Documentation generator", $$check_qdoc)

html-docs.commands = $$QDOC $$PWD/application-manager.qdocconf -outputdir $$BUILD_DIR/doc/html
html-docs.files = $$BUILD_DIR/doc/html

qch-docs.commands = $$QHELPGENERATOR $$BUILD_DIR/doc/html/appman.qhp -o $$BUILD_DIR/doc/qch/application-manager.qch
qch-docs.files = $$BUILD_DIR/doc/qch
qch-docs.CONFIG += no_check_exist directory

docs.depends = html-docs qch-docs

QMAKE_EXTRA_TARGETS += html-docs qch-docs docs
