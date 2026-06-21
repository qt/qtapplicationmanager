// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.SystemUI

// minimal System UI: just enough to bring up the ApplicationManager, PackageManager,
// WindowManager and Notifications singletons so their D-Bus interfaces can be tested, and to
// actually display application windows (so WindowManager.count / makeScreenshot have content)
Item {
    width: 400
    height: 300

    WindowItem {
        id: windowItem
        anchors.fill: parent
    }

    Connections {
        target: WindowManager
        function onWindowAdded(window) {
            windowItem.window = window;
        }
        function onWindowAboutToBeRemoved(window) {
            if (windowItem.window === window)
                windowItem.window = null;
        }
    }
}
