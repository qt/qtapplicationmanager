TARGET = qtyaml

load(am-config)

CONFIG += \
    static \
    hide_symbols \
    exceptions_off rtti_off warn_off \
    installed

load(qt_helper_lib)

win32-msvc* {
    QMAKE_CFLAGS += /D_CRT_SECURE_NO_WARNINGS
}
*-g++* {
    QMAKE_CFLAGS += -Wno-unused
}
*-clang* {
    CONFIG += warn_off
    QMAKE_CFLAGS += -Wall -W -Wno-unused
}

DEFINES *= YAML_DECLARE_STATIC HAVE_CONFIG_H

INCLUDEPATH += $$PWD/win32 $$PWD/include

SOURCES += \
    src/api.c \
    src/dumper.c \
    src/emitter.c \
    src/loader.c \
    src/parser.c \
    src/reader.c \
    src/scanner.c \
    src/writer.c \
