// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.11
import QtTest 1.0
import QtApplicationManager.SystemUI 2.0
import QtApplicationManager.Test

TestCase {
    id: testCase
    when: windowShown
    name: "WindowManager"
    visible: true

    Component {
        id: textComp
        Text {}
    }

    SignalSpy {
        id: windowManagerCompositorReadyChangedSpy
        target: ApplicationManager
        signalName: "windowManagerCompositorReadyChanged"
    }

    function test_addExtension() {
        if (!ApplicationManager.singleProcess) {
            if (!ApplicationManager.windowManagerCompositorReady) {
                var extnull = Qt.createComponent("IviApplicationExtension.qml").createObject(null).addExtension();
                compare(extnull, null);
                windowManagerCompositorReadyChangedSpy.wait(2000 * AmTest.timeoutFactor);
                verify(ApplicationManager.windowManagerCompositorReady);
            }
            var extension = Qt.createComponent("IviApplicationExtension.qml").createObject(null).addExtension();
            verify(extension);
            verify(extension.hasOwnProperty('iviSurfaceCreated'));
        }
        compare(WindowManager.addExtension(textComp), null);
    }
}
