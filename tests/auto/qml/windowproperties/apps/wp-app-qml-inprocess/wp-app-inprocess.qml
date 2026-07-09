// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtApplicationManager.Application
import QtQuick

ApplicationManagerWindow {
    id: root
    property int expectedSize: 0

    Connections {
        target: ApplicationInterface
        function onOpenDocument(str) {
            expectedSize = str
        }
    }

    onWindowPropertyChanged: {
        const allProperties = windowProperties()
        if (Object.keys(allProperties).length === expectedSize)
            setWindowProperty("BACKCHANNEL", allProperties)
    }
}
