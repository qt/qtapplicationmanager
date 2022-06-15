// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtApplicationManager.Application 2.0
import QtQuick 2.11
import appwidgets 1.0
import common 1.0

ApplicationManagerWindow {
    CommonObj { id: common }
    BlueRect { }
    GreenRect { }

    Timer {
        running: true
        interval: 20
        onTriggered: setWindowProperty("meaning", common.meaning);
    }
}
