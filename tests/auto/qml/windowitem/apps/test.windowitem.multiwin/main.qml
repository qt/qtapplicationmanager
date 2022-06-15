// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.4
import QtApplicationManager.Application 2.0

/*
 A simple test app that displays two windows
*/
QtObject {
    property var blueWindow: ApplicationManagerWindow {
        color: "blue"
        width: 123
        height: 321
    }
    property var orangeWindow: ApplicationManagerWindow {
        color: "orange"
        width: 321
        height: 123
    }
}
