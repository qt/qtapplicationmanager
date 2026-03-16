// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.Application

ApplicationManagerWindow {
    WindowContainer {
        x: 50; y: 30
        window: Window {
            width: 20; height: 20
            color: "#4682b4"
            visible: true
        }
    }
}
