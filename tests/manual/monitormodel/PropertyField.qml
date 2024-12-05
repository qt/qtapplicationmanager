// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    property string name
    property var object: null

    Label { text: root.name + ":" }
    TextField {
        id: field
        text: root.object ? root.object[root.name] : ""
    }
    Button {
        text: "Apply"
        onClicked: root.object[root.name] = field.text
    }
}
