TEMPLATE = aux

build_online_docs: {
    QMAKE_DOCS = $$PWD/online/applicationmanager.qdocconf
} else {
    QMAKE_DOCS = $$PWD/applicationmanager.qdocconf
}

OTHER_FILES += \
    *.qdocconf \
    *.qdoc \
