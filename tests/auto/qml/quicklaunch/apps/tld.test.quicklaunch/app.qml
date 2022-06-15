// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.0
import QtApplicationManager 2.0
import QtApplicationManager.Application 2.0

ApplicationManagerWindow {
    width: 320
    height: 240

    Component.onCompleted: {
        IntentClient.sendIntentRequest("quicklaunch-ready", { })
    }
}
