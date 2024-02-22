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

    SignalSpy {
        id: notificationManagerCountSpy
        target: NotificationManager
        signalName: "countChanged"
    }

    function cleanup() {
        for (let i = NotificationManager.count - 1; i >= 0; i--)
            NotificationManager.dismissNotification(NotificationManager.get(i).id);
        tryCompare(NotificationManager, "count", 0);
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

    function test_update() {
        compare(NotificationManager.count, 0);

        let ntfc = notificationComponent.createObject(testCase, { summary: "Noti", sticky: true });
        notificationManagerCountSpy.clear();
        ntfc.show();
        notificationManagerCountSpy.wait(1000 * AmTest.timeoutFactor);
        compare(NotificationManager.count, 1);
        let model0 = NotificationManager.get(0);
        const delta = new Date() - model0.created;
        verify(delta >= 0 && delta < 5000);
        verify(model0.created, model0.updated);

        ntfc.body = "Added";
        ntfc.extended = { key: "value" };
        tryCompare(NotificationManager.get(0), "body", "Added");
        model0 = NotificationManager.get(0);
        compare(model0.extended.key, "value");
        verify(model0.created < model0.updated);
    }

    function test_actions() {
        compare(NotificationManager.count, 0);

        let ntfc = notificationComponent.createObject(testCase,
                       { actions: [ { actId: "actText" }, { default: "" }, { default: "Default" } ] });
        notificationManagerCountSpy.clear();
        ntfc.show();
        notificationManagerCountSpy.wait(1000 * AmTest.timeoutFactor);
        compare(NotificationManager.count, 1);

        const model0 = NotificationManager.get(0);
        verify(model0.actions.length === 2);
        compare(Object.keys(model0.actions[0])[0], "actId");
        compare(model0.actions[0].actId, "actText");
        compare(Object.keys(model0.actions[1])[0], "default");
        compare(model0.actions[1].default, "Default");

        verify(model0.actionList.length === 2);
        compare(model0.actionList[0].actionId, "actId");
        compare(model0.actionList[0].actionText, "actText");
        compare(model0.actionList[1].actionId, "default");
        compare(model0.actionList[1].actionText, "Default");
    }
}
