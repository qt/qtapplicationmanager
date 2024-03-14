// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtTest
import QtApplicationManager.SystemUI
import QtApplicationManager.Test

TestCase {
    id: testCase
    when: windowShown
    name: "Native"

    WindowItem {
        id: windowItem
    }

    property int timeout: 5000 * AmTest.timeoutFactor

    function test_waylandExtension_data() {
        let data = [
            { tag: "qml-inprocess", appId: "wp-app-qml-inprocess" },
        ]
        if (!ApplicationManager.singleProcess) {
            data.push({ tag: "qml",    appId: "wp-app-qml" },
                      { tag: "native", appId: "wp-app-native" })
        }
        return data
    }

    function test_waylandExtension(data) {
        let app = ApplicationManager.application(data.appId);

        WindowManager.windowAdded.connect(function(window) {
            windowItem.window = window
        })
        // keep this sorted alphabetically ... otherwise the JSON.stringify comparison will fail
        let props = {
            "ARRAY": [ 1, 2, "3" ],
            "COLOR": Qt.color("blue"),
            "DATE": new Date(),
            "INT": 42,
            "OBJECT": { "a": 1, "b": 2 },
            "STRING": "str"
        }

        if (data.tag === "native") {
            // cannot serialize QColor via CBOR
            props["COLOR"] = props["COLOR"].toString()
        }

        verify(app.start("" + Object.keys(props).length))
        tryVerify(() => { return app.runState === Am.Running }, timeout)
        tryVerify(() => { return windowItem.window !== null }, timeout)
        for (const prop in props)
            windowItem.window.setWindowProperty(prop, props[prop])

        tryVerify(() => {
            return JSON.stringify(windowItem.window.windowProperty("BACKCHANNEL"))
                      === JSON.stringify(props)
        }, timeout)
        app.stop();
        tryVerify(() => { return app.runState === Am.NotRunning }, timeout)
        windowItem.window = null
    }
}
