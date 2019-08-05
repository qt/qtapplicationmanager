/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

import QtQuick 2.4
import QtTest 1.0
import QtApplicationManager 2.0
import QtApplicationManager.SystemUI 2.0

TestCase {
    id: testCase
    when: windowShown
    name: "Intents"

    property var stdParams: { "para": "meter" }
    property var matchParams: { "list": "a", "int": 42, "string": "foo_x_bar", "complex": { "a": 1 } }

    ListView {
        id: listView
        model: ApplicationManager
        delegate: Item {
            property var modelData: model
        }
    }

    function initTestCase() {
        verify(ApplicationManager.application("intents1"))
        verify(ApplicationManager.application("intents2"))
        if (!ApplicationManager.singleProcess)
            verify(ApplicationManager.application("cannot-start"))
    }

    SignalSpy {
        id: requestSpy
        target: null
        signalName: "replyReceived"
    }

    function test_intent_gadget() {
        var allIntents = IntentServer.intentList
        verify(allIntents.length > 0)

        // test intent properties
        var intent = IntentServer.find("both", "intents1")
        verify(intent.valid)
        compare(intent.intentId, "both")
        compare(intent.applicationId, "intents1")
        compare(intent.visibility, Intent.Public)
        compare(intent.requiredCapabilities, [])
        compare(intent.parameterMatch, {})

        // test comparison operators
        var pos = allIntents.indexOf(intent)
        verify(pos >= 0)
        var intent1 = IntentServer.find("both", "intents1")
        var intent2 =  IntentServer.find("both", "intents2")
        verify(intent1.valid)
        verify(intent2.valid)
        verify(intent1 == intent)
        verify(intent1 === intent)
        verify(intent1 != intent2)
        verify(intent1 !== intent2)
        verify(intent1 < intent2)
        verify(intent2 > intent1)

        verify(!IntentServer.find("both", "intents3").valid)
        verify(!IntentServer.find("bothx", "intents1").valid)
        verify(!IntentServer.find("both", "").valid)
        verify(!IntentServer.find("", "intents1").valid)
        verify(!IntentServer.find("", "").valid)
    }


    function test_match() {
        // first, check the matching on the server API
        var intent = IntentServer.find("match", "intents1")
        verify(!intent.valid)
        intent = IntentServer.find("match", "intents1", matchParams)
        verify(intent.valid)
        compare(intent.parameterMatch, { "list": [ "a", "b" ], "int": 42, "string": "^foo_.*_bar$", "complex": { "a": 1 } })

        var params = matchParams
        params.list = "c"
        verify(!IntentServer.find("match", "intents1", params).valid)
        params.list = "b"
        verify(IntentServer.find("match", "intents1", params).valid)

        params.int = 2
        verify(!IntentServer.find("match", "intents1", params).valid)
        params.int = 42

        params.string = "foo"
        verify(!IntentServer.find("match", "intents1", params).valid)
        params.string = "foo_test_bar"
        verify(IntentServer.find("match", "intents1", params).valid)

        params.complex = "string"
        verify(!IntentServer.find("match", "intents1", params).valid)
        params.complex = matchParams.complex
    }


    function test_intents_data() {
        return [
                    {tag: "1-1", intentId: "only1", appId: "intents1", succeeding: true },
                    {tag: "2-2", intentId: "only2", appId: "intents2", succeeding: true },
                    {tag: "1-2", intentId: "only1", appId: "intents2", succeeding: false,
                        errorMessage: "No matching IntentHandler found." },
                    {tag: "2-1", intentId: "only2", appId: "intents1", succeeding: false,
                        errorMessage: "No matching intent handler registered.",
                        ignoreWarning: 'Unknown intent "only2" was requested from application ":sysui:"' },
                    {tag: "match-1", intentId: "match", appId: "intents1", succeeding: true, params: matchParams },
                    {tag: "match-2", intentId: "match", appId: "intents1", succeeding: false,
                        params: function() { var x = Object.assign({}, matchParams); x.int = 1; return x }(),
                        errorMessage: "No matching intent handler registered.",
                        ignoreWarning: 'Unknown intent "match" was requested from application ":sysui:"' },
                    {tag: "match-3", intentId: "match", appId: "intents1", succeeding: false, params: {},
                        errorMessage: "No matching intent handler registered.",
                        ignoreWarning: 'Unknown intent "match" was requested from application ":sysui:"' },
                    {tag: "unknown-1", intentId: "unknown", appId: "intents1", succeeding: false,
                        errorMessage: "No matching intent handler registered.",
                        ignoreWarning: 'Unknown intent "unknown" was requested from application ":sysui:"' },
                    {tag: "unknown-2", intentId: "unknown", appId: "", succeeding: false,
                        errorMessage: "No matching intent handler registered.",
                        ignoreWarning: 'Unknown intent "unknown" was requested from application ":sysui:"' },
                    {tag: "custom-error", intentId: "custom-error", appId: "intents1", succeeding: false,
                        errorMessage: "custom error" },
                    {tag: "cannot-start", intentId: "cannot-start-intent", appId: "cannot-start", succeeding: false,
                        errorMessage: /Starting handler application timed out after .*/ },
                ];
    }

    function test_intents(data) {
        if (data.appId && !ApplicationManager.application(data.appId))
            skip("Application \"" + data.appId + "\" is not available")

        if (data.ignoreWarning)
            ignoreWarning(data.ignoreWarning)

        var params = ("params" in data) ? data.params : stdParams

        var req = IntentClient.sendIntentRequest(data.intentId, data.appId, params)
        verify(req)
        requestSpy.target = req
        tryCompare(requestSpy, "count", 1, 1000)
        compare(req.succeeded, data.succeeding)
        if (req.succeeded) {
            compare(req.result, { "from": data.appId, "in": params })
        } else {
            if (data.errorMessage instanceof RegExp)
                verify(data.errorMessage.test(req.errorMessage))
            else
                compare(req.errorMessage, data.errorMessage)
        }
        requestSpy.clear()
        requestSpy.target = null
    }

    function test_disambiguate_data() {
        return [
                    {tag: "no-signal", action: "none", succeeding: true },
                    {tag: "reject", action: "reject", succeeding: false,
                        errorMessage: "Disambiguation was rejected" },
                    {tag: "timeout", action: "timeout", succeeding: false,
                        errorMessage: /Disambiguation timed out after .*/ },
                    {tag: "ack-invalid", action: "acknowledge", acknowledgeIntentId: "only1", succeeding: false,
                        errorMessage: "Failed to disambiguate",
                        ignoreWarning: /IntentServer::acknowledgeDisambiguationRequest for intent .* tried to disambiguate to the intent "only1" which was not in the list of potential disambiguations/ },
                    {tag: "ack-valid", action: "acknowledge", succeeding: true }
                ];
    }

    SignalSpy {
        id: disambiguateSpy
        target: IntentServer
    }

    function test_disambiguate(data) {
        var intentId = "both"

        if (data.ignoreWarning)
            ignoreWarning(data.ignoreWarning)

        disambiguateSpy.signalName = data.action === "none" ? "" : "disambiguationRequest";

        var req = IntentClient.sendIntentRequest("both", stdParams)
        verify(req)
        requestSpy.target = req

        if (data.action !== "none") {
            tryCompare(disambiguateSpy, "count", 1, 1000)
            var possibleIntents = disambiguateSpy.signalArguments[0][1]
            compare(possibleIntents.length, 2)
            compare(possibleIntents[0].intentId, intentId)
            compare(possibleIntents[1].intentId, intentId)
            compare(possibleIntents[0].applicationId, "intents1")
            compare(possibleIntents[1].applicationId, "intents2")
            compare(disambiguateSpy.signalArguments[0][2], stdParams)

            switch (data.action) {
            case "reject":
                IntentServer.rejectDisambiguationRequest(disambiguateSpy.signalArguments[0][0])
                break
            case "acknowledge":
                var intent = possibleIntents[0]
                if (data.acknowledgeIntentId)
                    intent = IntentServer.find(data.acknowledgeIntentId, possibleIntents[0].applicationId)
                IntentServer.acknowledgeDisambiguationRequest(disambiguateSpy.signalArguments[0][0], intent)
                break
            }
            disambiguateSpy.clear()
        }

        tryCompare(requestSpy, "count", 1, data.action === "timeout" ? 15000 : 1000)
        var succeeding = data.succeeding
        compare(req.succeeded, succeeding)
        if (succeeding) {
            compare(req.errorMessage, "")
            verify(req.result !== {})
        } else {
            if (data.errorMessage instanceof RegExp)
                verify(data.errorMessage.test(req.errorMessage))
            else
                compare(req.errorMessage, data.errorMessage)
            compare(req.result, {})
        }
        requestSpy.clear()
    }
}
