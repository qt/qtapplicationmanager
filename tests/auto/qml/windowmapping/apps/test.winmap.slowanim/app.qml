// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.Application
import SlowAnimHelper

ApplicationManagerWindow {
    id: root
    color: "blue"

    SlowAnimHelper {
        id: helper
    }

    // Replies with this process' current animation speed modifier, so the test can verify that
    // a slowAnimations change made on the System-UI side actually reached the application process.
    IntentHandler {
        intentIds: [ "query-speed-modifier" ]
        onRequestReceived: (request) => {
            request.sendReply({ "speedModifier": helper.speedModifier() })
        }
    }
}
