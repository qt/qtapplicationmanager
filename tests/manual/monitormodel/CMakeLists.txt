# Copyright (C) 2024 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

qt_internal_add_manual_test(tst_monitormodel
    GUI
    EXCEPTIONS
    SOURCES
        main.cpp
    LIBRARIES
        Qt::AppManSystemUIPrivate
)

if(QT_KNOWN_POLICY_QTP0001)
    qt_policy(SET QTP0001 NEW)
endif()

qt_add_qml_module(tst_monitormodel
    URI "MonitorModel"
    QML_FILES
        PropertyField.qml
        SystemMonitorChart.qml
        main.qml
    RESOURCES
        am-config.yaml
)

if (QT_FEATURE_static)
    # This define flags us as an "appman" and allows us to link against the AppMan's private libraries
    target_compile_definitions(tst_monitormodel PRIVATE AM_COMPILING_APPMAN)
endif()
