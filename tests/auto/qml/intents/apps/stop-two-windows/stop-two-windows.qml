// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.Application
import QtApplicationManager.Test

// single-process only: AmTest is provided by the System UI's test runner
ApplicationManagerWindow {
    id: root
    width: 200
    height: 200

    Component.onCompleted: AmTest.observeObjectDestroyed(root)

    ApplicationManagerWindow {
        width: 100
        height: 100
    }
}
