// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtTest
import QtApplicationManager.SystemUI
import QtApplicationManager.Test

TestCase {
    id: root

    readonly property int delay: 0  // [ms] increase to follow what's going on
    readonly property int spyTimeout: 5000 * AmTest.timeoutFactor

    component WindowChrome: FocusScope {
        property alias winSurface: inner
        signal keyEvent(int key, bool press)

        width: inner.width
        height: inner.height

        WindowItem {
            id: inner
            focus: true

            Keys.onPressed: (event) => {
                keyEvent(event.key, true)
                if (event.key === Qt.Key_Left || event.key === Qt.Key_Right)
                    event.accepted = true;
            }

            Keys.onReleased: (event) => {
                keyEvent(event.key, false)
                if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                    txt.text = `System UI: ${event.key === Qt.Key_Up ? "UP" : "DOWN"}`;
                    tim.restart();
                } else if (event.key === Qt.Key_Left) {
                    leftWin.focus = true;
                    event.accepted = true;
                } else if (event.key === Qt.Key_Right) {
                    rightWin.focus = true;
                    event.accepted = true;
                } else {
                    console.info(`System UI: ${event.key}`);
                }
            }
        }

        Rectangle {
            anchors.fill: inner
            anchors.margins: -border.width
            border.width: 3
            border.color: parent.activeFocus ? "red" : "transparent"
            color: "transparent"

            Text {
                id: txt
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: 16
            }

            Timer {
                id: tim
                onTriggered: txt.text = "";
            }
        }
    }

    name: "KeyInput"
    width: 652
    height: 246
    visible: true
    when: windowShown

    WindowChrome {
        id: leftWin
        x: 3; y: 3
        focus: true
    }

    WindowChrome {
        id: rightWin
        x: 329; y: 3
    }

    Connections {
        target: WindowManager

        function onWindowAdded(window) {
            if (window.application.id === "app1")
                leftWin.winSurface.window = window;
            else if (window.application.id === "app2")
                rightWin.winSurface.window = window;
        }

        function onWindowContentStateChanged(window) {
            if (window.contentState === WindowObject.NoSurface) {
                if (window.application.id === "app1")
                    leftWin.winSurface.window = null;
                else if (window.application.id === "app2")
                    rightWin.winSurface.window = null;
            }
        }
    }

    IntentServerHandler {
        id: intentHandler
        intentIds: [ "copythat" ]
        visibility: IntentObject.Public
        //onRequestReceived: (request) => { console.info(`System UI: key: ${request.parameters.key} ack.`); }
    }

    SignalSpy {
        id: copyThatSpy
        target: intentHandler
        signalName: "requestReceived"
    }

    SignalSpy {
        id: keyEventLeftSpy
        target: leftWin
        signalName: "keyEvent"
    }

    SignalSpy {
        id: keyEventRightSpy
        target: rightWin
        signalName: "keyEvent"
    }

    function click(key) {
        keyPress(key);
        AmTest.aboutToBlock();      // see QTBUG-83422
        wait(50);
        keyRelease(key);
        AmTest.aboutToBlock();
        wait(root.delay);
    }

    function test_moveFocus() {
        skip("Not working ATM: ASSERT on Linux/multi-process and missing signal on macOS/single-process")

        if (root.Window.window.flags & Qt.WindowDoesNotAcceptFocus)
            skip("Test can only be run without AM_BACKGROUND_TEST set, since it requires input focus");

        ApplicationManager.startApplication("app1");
        ApplicationManager.startApplication("app2");

        tryVerify(function() {
                      return leftWin.winSurface.window != null && rightWin.winSurface.window != null
                  }, spyTimeout);

        wait(100);

        //console.info("Inject Up");
        click(Qt.Key_Up);
        copyThatSpy.wait(spyTimeout);
        compare(copyThatSpy.signalArguments[0][0].parameters.app, 1);
        compare(copyThatSpy.signalArguments[0][0].parameters.key, Qt.Key_Up);
        keyEventLeftSpy.wait(spyTimeout);
        compare(keyEventLeftSpy.signalArguments[0][0], Qt.Key_Up);
        verify(keyEventLeftSpy.signalArguments[0][1]);
        keyEventLeftSpy.wait(spyTimeout);
        compare(keyEventLeftSpy.signalArguments[1][0], Qt.Key_Up);
        verify(!keyEventLeftSpy.signalArguments[1][1]);

        //console.info("Inject Right");
        click(Qt.Key_Right);
        copyThatSpy.wait(spyTimeout);     // unfortunately keys always go to clients first
        compare(copyThatSpy.signalArguments[1][0].parameters.app, 1);
        compare(copyThatSpy.signalArguments[1][0].parameters.key, Qt.Key_Right);
        keyEventLeftSpy.wait(spyTimeout);
        compare(keyEventLeftSpy.signalArguments[2][0], Qt.Key_Right);
        verify(keyEventLeftSpy.signalArguments[2][1]);
        keyEventLeftSpy.wait(spyTimeout);
        compare(keyEventLeftSpy.signalArguments[3][0], Qt.Key_Right);
        verify(!keyEventLeftSpy.signalArguments[3][1]);

        //console.info("Inject Down");
        click(Qt.Key_Down);
        copyThatSpy.wait(spyTimeout);
        compare(copyThatSpy.signalArguments[2][0].parameters.app, 2);
        compare(copyThatSpy.signalArguments[2][0].parameters.key, Qt.Key_Down);
        keyEventRightSpy.wait(spyTimeout);
        compare(keyEventRightSpy.signalArguments[0][0], Qt.Key_Down);
        verify(keyEventRightSpy.signalArguments[0][1]);
        keyEventRightSpy.wait(spyTimeout);
        compare(keyEventRightSpy.signalArguments[1][0], Qt.Key_Down);
        verify(!keyEventRightSpy.signalArguments[1][1]);

        //console.info("Inject Left");
        click(Qt.Key_Left);
        copyThatSpy.wait(spyTimeout);     // unfortunately keys always go to clients first
        compare(copyThatSpy.signalArguments[3][0].parameters.app, 2);
        compare(copyThatSpy.signalArguments[3][0].parameters.key, Qt.Key_Left);
        keyEventRightSpy.wait(spyTimeout);
        compare(keyEventRightSpy.signalArguments[2][0], Qt.Key_Left);
        verify(keyEventRightSpy.signalArguments[2][1]);
        keyEventRightSpy.wait(spyTimeout);
        compare(keyEventRightSpy.signalArguments[3][0], Qt.Key_Left);
        verify(!keyEventRightSpy.signalArguments[3][1]);

        //console.info("Inject Down");
        click(Qt.Key_Down);
        copyThatSpy.wait(spyTimeout);
        compare(copyThatSpy.signalArguments[4][0].parameters.app, 1);
        compare(copyThatSpy.signalArguments[4][0].parameters.key, Qt.Key_Down);
        keyEventLeftSpy.wait(spyTimeout);
        compare(keyEventLeftSpy.signalArguments[4][0], Qt.Key_Down);
        verify(keyEventLeftSpy.signalArguments[4][1]);
        keyEventLeftSpy.wait(spyTimeout);
        compare(keyEventLeftSpy.signalArguments[5][0], Qt.Key_Down);
        verify(!keyEventLeftSpy.signalArguments[5][1]);
    }
}
