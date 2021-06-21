/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.11
import QtApplicationManager.Application 2.0
import QtWayland.Compositor 1.3
import QtWayland.Compositor.XdgShell
import QtWayland.Compositor.WlShell

ApplicationManagerWindow {
    id: root
    color: "lightgrey"

    property ListModel shellSurfaces: ListModel {}

    Text {
        anchors.fill: parent
        anchors.margins: 8
        font.pointSize: 14
        wrapMode: Text.Wrap
        textFormat: Text.RichText
        text: "This Wayland<sup>*</sup> client window implements a Wayland compositor (nested compositor). " +
              "To display Wayland clients here, set:<br><br><b>WAYLAND_DISPLAY=qtam-wayland-nested</b>" +
              "<br><br>For instance:<br>WAYLAND_DISPLAY=qtam-wayland-nested qmlscene client.qml -platform wayland" +
              "<br><br><small>* in multi-process mode</small>"
    }

    WaylandCompositor {
        socketName: "qtam-wayland-nested"

        WaylandOutput {
            window: root
            sizeFollowsWindow: true
        }

        WlShell {
            onWlShellSurfaceCreated: shellSurfaces.append({shellSurface: shellSurface});
        }

        XdgShell {
            onToplevelCreated: shellSurfaces.append({shellSurface: xdgSurface});
        }
    }

    Repeater {
        model: shellSurfaces
        ShellSurfaceItem {
            shellSurface: modelData
            onSurfaceDestroyed: shellSurfaces.remove(index)
        }
    }

    Component.onCompleted: console.info("Start a client application in the nested compositor for instance with:\n  " +
                                        "WAYLAND_DISPLAY=qtam-wayland-nested QT_WAYLAND_DISABLE_WINDOWDECORATION=1 " +
                                        "QT_WAYLAND_SHELL_INTEGRATION=xdg-shell qmlscene client.qml -platform wayland");
}
