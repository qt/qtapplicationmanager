// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

pragma ComponentBehavior: Bound

import QtQuick
import QtApplicationManager.Application
import QtWayland.Compositor
import QtWayland.Compositor.XdgShell
import QtWayland.Compositor.WlShell

Item {
    id: root

    function close() {
        shellSurfaces.clear();
    }

    Text {
        anchors.fill: parent
        anchors.margins: 8
        font.pointSize: 14
        wrapMode: Text.Wrap
        textFormat: Text.RichText
        text: "This Wayland<sup>*</sup> client window implements a Wayland compositor (nested compositor). " +
              "To display Wayland clients here, set:<br><br><b>WAYLAND_DISPLAY=qtam-wayland-nested</b>" +
              "<br><br>For instance:<br>WAYLAND_DISPLAY=qtam-wayland-nested qml client.qml -platform wayland" +
              "<br><br><small>* in multi-process mode</small>"
    }

    Loader {
        id: ldr
        active: root.Window.window       // Window attached property might not be available immediately (in-process)
        sourceComponent: Component {
            WaylandCompositor {
                socketName: "qtam-wayland-nested"

                WaylandOutput {
                    window: root.Window.window
                    sizeFollowsWindow: true
                }

                WlShell {
                    onWlShellSurfaceCreated: (shellSurface) => shellSurfaces.append({shellSurface});
                }

                XdgShell {
                    onToplevelCreated: (toplevel, xdgSurface) => shellSurfaces.append({xdgSurface});
                }
            }
        }
    }

    Repeater {
        model: ListModel { id: shellSurfaces }
        ShellSurfaceItem {
            required property var modelData
            required property int index
            shellSurface: modelData
            onSurfaceDestroyed: shellSurfaces.remove(index)
        }
    }

    Connections {
        target: ApplicationInterface
        function onQuit() {
            root.close();
            target.acknowledgeQuit();
        }
    }

    Component.onCompleted: console.info("Start a client application in the nested compositor for instance with:\n  " +
                                        "WAYLAND_DISPLAY=qtam-wayland-nested QT_WAYLAND_DISABLE_WINDOWDECORATION=1 " +
                                        "QT_WAYLAND_SHELL_INTEGRATION=xdg-shell qml client.qml -platform wayland");
}
