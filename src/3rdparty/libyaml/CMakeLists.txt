
qt_internal_add_3rdparty_library(BundledLibYaml
    QMAKE_LIB_NAME yaml
    STATIC
    INSTALL
    SOURCES
        src/api.c
        src/dumper.c
        src/emitter.c
        src/loader.c
        src/parser.c
        src/reader.c
        src/scanner.c
        src/writer.c
    DEFINES
        HAVE_CONFIG_H
        YAML_DECLARE_STATIC
        YAML_DECLARE_EXPORT
    PUBLIC_DEFINES
        YAML_DECLARE_STATIC
    INCLUDE_DIRECTORIES
        include
        win32
    PUBLIC_INCLUDE_DIRECTORIES
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

qt_disable_warnings(BundledLibYaml)
qt_set_symbol_visibility_hidden(BundledLibYaml)
