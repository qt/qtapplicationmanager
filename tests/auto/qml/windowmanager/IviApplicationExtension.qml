// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

pragma ComponentBehavior: Bound
import QtQuick 2.11
import QtApplicationManager.SystemUI 2.0
import QtWayland.Compositor 1.3
import QtWayland.Compositor.IviApplication

QtObject {
    property Component iviComp: Component {
        IviApplication {}
    }

    function addExtension() {
        return WindowManager.addExtension(iviComp);
    }
}
