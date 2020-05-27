TEMPLATE = subdirs
qtHaveModule(waylandclient):qtHaveModule(widgets) {
    SUBDIRS = widgets
}
