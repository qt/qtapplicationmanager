
TEMPLATE = aux

load(am-config)

HEADERS = \
    startupinterface.h \
    containerinterface.h

OTHER_FILES = \
    qtapplicationmanager-plugin-interfaces.pc.in

headers.path = $$INSTALL_PREFIX/include/qtapplicationmanager/plugin-interfaces
headers.files = $$HEADERS

pcin.input = $$PWD/qtapplicationmanager-plugin-interfaces.pc.in
pcin.output = $$OUT_PWD/qtapplicationmanager-plugin-interfaces.pc
QMAKE_SUBSTITUTES += pcin

pkgconfig.path = $$INSTALL_PREFIX/share/pkgconfig
pkgconfig.files = $$pcin.output

INSTALLS += headers pkgconfig
