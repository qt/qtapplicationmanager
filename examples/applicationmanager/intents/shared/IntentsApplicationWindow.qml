/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
**
** $QT_BEGIN_LICENSE:BSD-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
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
** SPDX-License-Identifier: BSD-3-Clause
**
****************************************************************************/

import QtQuick 2.11
import QtApplicationManager.Application 2.0
import QtApplicationManager 2.0

ApplicationManagerWindow {
    id: root
    property string flavor: ApplicationInterface.applicationId.split(".")[1]

    IntentsUIPage {
        id: intentPage
        anchors.fill: parent
        flavor: root.flavor
        title: ApplicationInterface.name["en"]

        //! [Send Intent]
        onRequest: {
            var request = IntentClient.sendIntentRequest(intentId, applicationId, parameters)
            request.onReplyReceived.connect(function() {
                intentPage.setResult(request.requestId, request.succeeded,
                                     request.succeeded ? request.result : request.errorMessage)
            })
        }
        //! [Send Intent]

        //! [Intent Animation]
        RotationAnimation on rotation {
            id: rotationAnimation
            running: false
            duration: 500; from: 0; to: 360
        }
        //! [Intent Animation]

        SequentialAnimation  on scale {
            id: scaleAnimation
            running: false
            PropertyAnimation { from: 1; to: 0.5 }
            PropertyAnimation { from: 0.5; to: 1 }
        }

        SequentialAnimation on color {
            id: blinkAnimation
            running: false
            ColorAnimation { to: "black" }
            ColorAnimation { to: "white" }
        }

        SequentialAnimation on color {
            id: blueAnimation
            running: false
            ColorAnimation { to: "blue"; duration: 2000 }
            ColorAnimation { to: "white" }
        }
    }

    //! [Intent Handler]
    IntentHandler {
        intentIds: "rotate-window"
        onRequestReceived: {
            rotationAnimation.start()
            request.sendReply({ "done": true })
        }
    }
    //! [Intent Handler]

    IntentHandler {
        intentIds: "scale-window"
        onRequestReceived: {
            scaleAnimation.start()
            request.sendReply({ "done": true })
        }
    }

    IntentHandler {
        intentIds: "blink-window"
        onRequestReceived: {
            blinkAnimation.start()
            request.sendReply({ "done": true })
        }
    }

    IntentHandler {
        intentIds: "blue-window-private"
        onRequestReceived: {
            blueAnimation.start()
            request.sendReply({ "done": true })
        }
    }
}
