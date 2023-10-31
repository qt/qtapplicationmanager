// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtTest
import QtApplicationManager.SystemUI
import QtApplicationManager.Test

TestCase {
    id: testCase
    name: "Notifications"

    Component {
        id: notificationComponent
        Notification {}
    }

    NotificationModel {
        id: notificationModel
        sortFunction: function(ln, rn) { return ln.priority < rn.priority; }
    }

    SignalSpy {
        id: notificationModelCountSpy
        target: notificationModel
        signalName: "countChanged"
    }

    function test_notificationModel() {
        compare(notificationModel.count, 0);

        let ntfc = [];
        for (let i = 0; i < 3; i++) {
            ntfc[i] = notificationComponent.createObject(testCase, { summary: `N${i}`, sticky: true });
            ntfc[i].priority = i === 1 ? 7 : i;
            notificationModelCountSpy.clear();
            ntfc[i].show();
            notificationModelCountSpy.wait(1000 * AmTest.timeoutFactor);
            compare(notificationModel.count, i + 1);
        }

        compare(notificationModel.indexOfNotification(ntfc[0].notificationId), 0);
        compare(notificationModel.indexOfNotification(ntfc[1]), 2);
        compare(notificationModel.indexOfNotification(ntfc[2].notificationId), 1);

        notificationModelCountSpy.clear();
        notificationModel.filterFunction = function(n) { return n.summary !== "N0"; };

        notificationModelCountSpy.wait(1000 * AmTest.timeoutFactor);
        compare(notificationModel.count, 2);
        compare(notificationModel.indexOfNotification(ntfc[2]), 0);

        ntfc[1].summary = "N0";
        notificationModelCountSpy.wait(1000 * AmTest.timeoutFactor);
        compare(notificationModel.count, 1);
    }
}
