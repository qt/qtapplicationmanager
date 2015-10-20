
TEMPLATE = subdirs

include($$BASE_PRI)

!android:!windows:!osx:SUBDIRS = qml
