// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick 2.4
import QtQuick.Controls 2.4

Label {
    property var value
    property string name
    text: name + ": " + (value / 1e6).toFixed(0) + " MB"
    font.pixelSize: 15
}
