
exists($$[QT_INSTALL_BINS]/qdoc):exists($$[QT_INSTALL_BINS]/qhelpgenerator) {
    check_qdoc = "qdoc/qhelpgenerator in $$[QT_INSTALL_BINS]"
    QDOC = $$[QT_INSTALL_BINS]/qdoc
    QHELPGENERATOR = $$[QT_INSTALL_BINS]/qhelpgenerator
} else {
    check_qdoc = "qdoc/qhelpgenerator in PATH"
    QDOC = qdoc
    QHELPGENERATOR = qhelpgenerator
}

VERSION_TAG = $$replace(VERSION, "[-.]", )

defineReplace(cmdEnv) {
    !equals(QMAKE_DIR_SEP, /): 1 ~= s,^(.*)$,(set \\1) &&,g
    return("$$1")
}

defineReplace(qdoc) {
    return("$$cmdEnv(OUTDIR=$$1 APPMAN_VERSION=$$VERSION APPMAN_VERSION_TAG=$$VERSION_TAG QT_INSTALL_DOCS=$$[QT_INSTALL_DOCS/src]) $$QDOC")
}

printConfigLine("Documentation generator", $$check_qdoc)

html-docs.commands = $$qdoc($$BUILD_DIR/doc/html) $$PWD/applicationmanager.qdocconf
html-docs.files = $$BUILD_DIR/doc/html

html-docs-online.commands = $$qdoc($$BUILD_DIR/doc/html) $$PWD/applicationmanager-online.qdocconf
html-docs-online.files = $$BUILD_DIR/doc/html

qch-docs.commands = $$QHELPGENERATOR $$BUILD_DIR/doc/html/applicationmanager.qhp -o $$BUILD_DIR/doc/qch/application-manager.qch
qch-docs.files = $$BUILD_DIR/doc/qch
qch-docs.CONFIG += no_check_exist directory

docs.depends = html-docs qch-docs
docs-online.depends = html-docs-online

QMAKE_EXTRA_TARGETS += html-docs qch-docs docs html-docs-online docs-online

OTHER_FILES += \
    $$PWD/application-manager.qdocconf \
    $$PWD/*.qdoc
