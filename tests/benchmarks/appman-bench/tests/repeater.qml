// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick 2.4

Grid {
    id: grid
    property int count: 10000
    columns: Math.ceil(Math.sqrt(count))

    Repeater {
        model: grid.count

        Rectangle {
            width: grid.width / grid.columns;
            height: grid.height / Math.ceil(grid.count / grid.columns)

            color: Qt.rgba(Math.random(),Math.random(),Math.random(),1);
        }
    }
}
