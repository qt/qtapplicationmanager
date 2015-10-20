
# Since the application manager does not provide any library for plugins to link against and since QMAKE
# does not let us specify the list of libraries to add to the pkgconfig file, we create an empty library.

include($$BASE_PRI)

TEMPLATE = lib
TARGET = application-manager

CONFIG += create_prl create_pc

target.path = $$INSTALL_PREFIX/lib

QMAKE_PKGCONFIG_NAME = ApplicationManager
QMAKE_PKGCONFIG_DESCRIPTION = Application manager
QMAKE_PKGCONFIG_PREFIX = $$INSTALL_PREFIX
QMAKE_PKGCONFIG_DESTDIR = pkgconfig
QMAKE_PKGCONFIG_VERSION = $$VERSION
pkgconfig.files = pkgconfig/application-manager.pc
pkgconfig.path = $$INSTALL_PREFIX/lib/pkgconfig

INSTALLS += target pkgconfig
