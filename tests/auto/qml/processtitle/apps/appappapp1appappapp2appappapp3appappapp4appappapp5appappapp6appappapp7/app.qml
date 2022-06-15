// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2020 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.15
import QtApplicationManager.Application 2.0

Connections {
    target: ApplicationInterface
    function onQuit() {
        target.acknowledgeQuit();
    }
}
