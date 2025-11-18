// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
import QtQuick
import QtApplicationManager.Application
import TestApp

ApplicationManagerWindow {
    width: 320
    height: 240

    Helper {
        id: helper
    }

    Component.onCompleted: {
        IntentClient.sendIntentRequest("app-started", {
                                           "env": {
                                               "FOO": helper.getEnv("FOO"),
                                               "BAR": helper.getEnv("BAR"),
                                               "BAZ": helper.getEnv("BAZ"),
                                               "BAD": helper.getEnv("BAD"),
                                               "BAD_TWO": helper.getEnv("BAD_TWO")
                                           }
                                       })
    }
}
