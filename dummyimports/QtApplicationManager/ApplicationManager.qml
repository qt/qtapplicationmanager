/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

pragma Singleton
import QtQuick 2.2

ListModel {
    id: root

    property int count: 3
    property bool dummy: true

    property var applibrary : [
        ["com.pelagicore.browser","icon.png", "Browser", "browser", "Browser.qml"],
        ["com.pelagicore.movies","icon.png", "Movies", "media", "Movies.qml"],
        ["com.pelagicore.music","icon.png", "Music", "media", "Music.qml"],
    ]

    signal emitSurface(int index, Item item)



    function setApplicationAudioFocus(appId, appFocus)
    {
        print("setApplicationAudioFocus(" + appId + ", " + appFocus + ")")
    }

    function qmltypeof(obj, className) { // QtObject, string -> bool
      // className plus "(" is the class instance without modification
      // className plus "_QML" is the class instance with user-defined properties
      var str = obj.toString();
      return str.indexOf(className + "(") == 0 || str.indexOf(className + "_QML") == 0;
    }

    function startApplication(id) {
        print("Starting the application. ")
        var component
        var item
        for (var i = 0; i < root.count; i++) {
            var entry
            print("ApplicationManager: " + createAppEntry(i).applicationId + " given app id to open: " + id)
            if (root.get(i).applicationId === id) {
                component = Qt.createComponent("../../../../apps/" + createAppEntry(i).applicationId + "/" + createAppEntry(i).qml);
                if (component.status === Component.Ready) {
                    item = component.createObject()
                    if (!item)
                        console.log("Failed to create an Object.")
                    else {
                        print("Starting the application. Sending a signal", i, item, item.children.length)
                        root.setProperty(i, "surfaceItem", item)
                        root.emitSurface(i, item)
                    }
                }
                else
                    console.log("Component creation failed " + createAppEntry(i).qml + " Error: " + component.errorString())
            }
        }
    }



    function createAppEntry(i) {

        var entry = {
            applicationId: applibrary[i][0],
            icon: "../../apps/" + applibrary[i][0] + "/" + applibrary[i][1],
            name: applibrary[i][2],
            isRunning: false,
            isStarting: false,
            isActive: false,
            isBlocked: false,
            isUpdating: false,
            isRemovable: false,
            updateProgress: 0,
            codeFilePath: "",
            categories: applibrary[i][3],
            qml: applibrary[i][4],
            surfaceItem: null
        }
        return entry
    }
    Component.onCompleted: {
        clear()
        for (var i = 0; i < root.count; i++) {
            append(createAppEntry(i))
        }
    }

}
