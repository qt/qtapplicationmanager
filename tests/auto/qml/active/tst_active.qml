// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtQuick.Controls.Basic
import QtTest
import QtApplicationManager.SystemUI

TestCase {
    id: root
    name: "Active"
    when: windowShown
    visible: true

    width: 360
    height: 240

    FocusScope {
        width: 120
        height: 240
        id: content
        objectName: "sysui.content"
        focus: false

        TextField {
            id: textField
            objectName: "sysui.textField"
            width: content.width
            height: content.height / 3
            text: "Text SysUI"
            focus: true
        }
        Button {
            id: button
            objectName: "sysui.button"
            y: textField.height
            width: content.width
            height: content.height / 3
            text: "Btn SysUI"
        }
    }

    WindowItem {
        id: app1Win
        x: 120
    }
    WindowItem {
        id: app2Win
        x: 240
    }

    Connections {
        target: WindowManager

        function onWindowAdded(window) {
            if (window.application.id === "app1")
                app1Win.window = window
            else if (window.application.id === "app2")
                app2Win.window = window
        }

        function onWindowContentStateChanged(window) {
            if (window.contentState === WindowObject.NoSurface) {
                if (window.application.id === "app1")
                    app1Win.window = null
                else if (window.application.id === "app2")
                    app2Win.window = null
            }
        }
    }

    function test_focus()
    {
        if (root.Window.window.flags & Qt.WindowDoesNotAcceptFocus)
            skip("Test can only be run without AM_BACKGROUND_TEST set, since it requires input focus");

        // the starting focus is inconsistent between Linux and macOS/Windows, so we force it

        content.forceActiveFocus()
        verify(Window.activeFocusItem == textField)


        // start app1, focus the window and check if app1's textField has focus now

        ApplicationManager.startApplication("app1")
        tryVerify(() => (app1Win.window?.application?.id === "app1"))

        verify(Window.activeFocusItem == textField)
        app1Win.forceActiveFocus()

        let afi1Req = IntentClient.sendIntentRequest("activeFocusItem", "app1", { "waitUntilActive": 1000 })
        tryCompare(afi1Req, "succeeded", true)
        compare(afi1Req.result.active, true)
        compare(afi1Req.result.amwindow, "app1.textField")
        compare(afi1Req.result.qwindow, "app1.textField")

        let afiBeforeApp2 = Window.activeFocusItem

        if (ApplicationManager.singleProcess)
            compare(Window.activeFocusItem?.objectName, "app1.textField")
        else
            compare(Window.activeFocusItem, app1Win.backingItem)


        // start app2, focus the window and check if app2's textField has focus now

        ApplicationManager.startApplication("app2")
        tryVerify(() => (app2Win.window?.application?.id === "app2"))

        verify(Window.activeFocusItem == afiBeforeApp2)
        app2Win.forceActiveFocus()

        let afi2Req = IntentClient.sendIntentRequest("activeFocusItem", "app2", { "waitUntilActive": 1000 })

        tryCompare(afi2Req, "succeeded", true)
        compare(afi2Req.result.active, true)
        compare(afi2Req.result.amwindow, "app2.textField")
        compare(afi2Req.result.qwindow, "app2.textField")

        afi1Req = IntentClient.sendIntentRequest("activeFocusItem", "app1", { })
        tryCompare(afi1Req, "succeeded", true)
        compare(afi1Req.result.active, false)
        compare(afi1Req.result.amwindow, undefined)

        if (ApplicationManager.singleProcess)
            compare(Window.activeFocusItem?.objectName, "app2.textField")
        else
            compare(Window.activeFocusItem, app2Win.backingItem)


        // click into the empty area at the bottom of app1 to move the focus to app1

        mouseClick(app1Win, 10, app1Win.height - 10)

        afi1Req = IntentClient.sendIntentRequest("activeFocusItem", "app1", { "waitUntilActive": 1000 })

        tryCompare(afi1Req, "succeeded", true)
        compare(afi1Req.result.active, true)
        compare(afi1Req.result.amwindow, "app1.textField")
        compare(afi1Req.result.qwindow, "app1.textField")

        afi2Req = IntentClient.sendIntentRequest("activeFocusItem", "app2", { })
        tryCompare(afi2Req, "succeeded", true)
        compare(afi2Req.result.active, false)

        if (ApplicationManager.singleProcess)
            compare(Window.activeFocusItem?.objectName, "app1.textField")
        else
            compare(Window.activeFocusItem, app1Win.backingItem)


        // click into the empty area at the bottom of the sys-ui -> the focus should NOT move

        mouseClick(content, 10, content.height - 10)
        wait(100 * AmTest.timeoutFactor)

        if (ApplicationManager.singleProcess)
            compare(Window.activeFocusItem?.objectName, "app1.textField")
        else
            compare(Window.activeFocusItem, app1Win.backingItem)

        // click into the sys-ui's textField to move the focus to the sys-ui

        mouseClick(content, 10, 10)

        compare(Window.activeFocusItem, textField)

        afi1Req = IntentClient.sendIntentRequest("activeFocusItem", "app1", { "waitUntilInactive": 1000 })
        tryCompare(afi1Req, "succeeded", true)
        compare(afi1Req.result.active, false)

        afi2Req = IntentClient.sendIntentRequest("activeFocusItem", "app2", { })
        tryCompare(afi2Req, "succeeded", true)
        compare(afi2Req.result.active, false)
    }
}
