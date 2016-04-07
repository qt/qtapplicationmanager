/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

var count = 3
var dummy = true

var additionalConfiguration = { }

function get(index) {
//    print("Type of the index " + typeof index)
    if (typeof index === 'number')
        return createAppEntry(index)
    else if (typeof index === 'string') {
        for (var i = 0; i < applibrary.length; i++) {
//            print(":::ApplicationManager::: running through the loop: " + index + " applibrary " + applibrary[i][0])
            if (applibrary[i][0] === index) {
                return createAppEntry(i)
            }
        }
    }
}


function setApplicationAudioFocus(appId, appFocus)
{
    print("setApplicationAudioFocus(" + appId + ", " + appFocus + ")")
}



function startApplication(id) {
    print("Starting the application. ")
    var component
    var item
    for (var i = 0; i < applibrary.length; i++) {
        var entry
        //print("ApplicationManager: " + createAppEntry(i).applicationId + " given app id to open: " + id)
        if (createAppEntry(i).applicationId == id) {
            component = Qt.createComponent("../../../../ui/apps/" + createAppEntry(i).name + "/" + createAppEntry(i).qml);
            if (component.status == Component.Ready) {
                item = component.createObject()
                if (!item)
                    console.log("Failed to create an Object.")
                else {
                    print("Starting the application. Sending a signal", i, item)
                    WindowManager.surfaceItems[i] = item
                    WindowManager.surfaceItemReady(i, item)
                }
            }
            else
                console.log("Component creation failed " + createAppEntry(i).qml + " Error: " + component.errorString())
        }
    }
}

var applibrary=[

            ["com.pelagicore.browser","icon.png", "Browser", "browser", "Browser.qml"],
            ["com.pelagicore.movies","icon.png", "Movies", "media", "Movies.qml"],
            ["com.pelagicore.music","icon.png", "Music", "media", "Music.qml"],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],
            ["","", "", ""],

        ];

function createAppEntry(i) {


    var entry = {
        applicationId: applibrary[i][0],
        icon: "/path/to/icon/" + applibrary[i][2] + "/" + applibrary[i][1],
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
