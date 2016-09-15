build_online_docs: {
    QMAKE_DOCS_TARGETDIR = applicationmanager
    QMAKE_DOCS = $$PWD/applicationmanager-online.qdocconf
} else {
    QMAKE_DOCS = $$PWD/applicationmanager.qdocconf
}

CONFIG += prepare_docs
load(qt_docs_targets)

OTHER_FILES += \
    $$PWD/*.qdocconf \
    $$PWD/*.qdoc \
    $$PWD/images/*.png
