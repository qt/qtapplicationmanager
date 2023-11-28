// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

import QtQuick


ParallelAnimation {
    property string mode
    property alias scaleTarget: scalani.target
    property alias colorTarget: colani.target

    running: mode.length > 0

    PropertyAnimation {
        id: scalani
        property: "scale"
        to: 0.0
        easing.type: Easing.OutInBounce
        easing.amplitude: 2
        duration: 1000
    }

    ColorAnimation  {
        id: colani;
        property: "color"
        to: "white"
        easing.type: Easing.OutInBounce
        easing.amplitude: 2
        duration: 1000;
    }
}
