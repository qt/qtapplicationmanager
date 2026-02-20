// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
import QtQuick
import QtApplicationManager.Application
import BwrapHelper

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
        IntentClient.sendIntentRequest("mount-test-result", {
                                           "ro-marker":    helper.readFile("/bwrap-ro-mount/marker.txt"),
                                           "appid-marker": helper.readFile("/bwrap-appid-mount/marker.txt"),
                                           "rw-writable":  helper.canWrite("/bwrap-rw-mount"),
                                           "cap-present":  helper.pathExists("/bwrap-cap-mount"),
                                           "cap-marker":   helper.readFile("/bwrap-cap-mount/marker.txt"),
                                           "app-path":     helper.pathExists("/custom-app/info.yaml"),
                                           "extra-dirs":   ApplicationInterface.extraDirs,
                                           "extra-dirs-accessible": ApplicationInterface.extraDirs["testdata"]
                                               ? helper.pathExists(ApplicationInterface.extraDirs["testdata"])
                                               : false
                                       })
    }
}
