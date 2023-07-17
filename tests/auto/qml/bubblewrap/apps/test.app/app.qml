// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.Application

ApplicationManagerWindow {
    width: 320
    height: 240

    Component.onCompleted: {
        IntentClient.sendIntentRequest("app-started", { })
    }
}
