// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.4
import QtTest 1.0
import QtApplicationManager 2.1
import QtApplicationManager.SystemUI 2.1

TestCase {
    id: testCase
    when: windowShown
    name: "Intents"

    property int spyTimeout: 1000 * AmTest.timeoutFactor

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
        verify(ApplicationManager.application("intents2.1"))
        if (!ApplicationManager.singleProcess)
            verify(ApplicationManager.application("cannot-start"))
    }

    SignalSpy {
        id: requestSpy
        target: null
        signalName: "replyReceived"
    }

    function test_intent_object() {
        verify(IntentServer.count> 0)

        // test intent properties
        var intent = IntentServer.applicationIntent("both", "intents1")
        verify(intent)
        compare(intent.intentId, "both")
        compare(intent.applicationId, "intents1")
        compare(intent.visibility, IntentObject.Public)
        compare(intent.requiredCapabilities, [])
        compare(intent.parameterMatch, {})

        verify(!IntentServer.applicationIntent("both", "intents3"))
        verify(!IntentServer.applicationIntent("bothx", "intents1"))
        verify(!IntentServer.applicationIntent("both", ""))
        verify(!IntentServer.applicationIntent("", "intents1"))
        verify(!IntentServer.applicationIntent("", ""))
    }


    function test_match() {
        // first, check the matching on the server API
        var intent = IntentServer.applicationIntent("match", "intents1")
        verify(!intent)
        intent = IntentServer.applicationIntent("match", "intents1", matchParams)
        verify(intent)
        compare(intent.parameterMatch, { "list": [ "a", "b" ], "int": 42, "string": "^foo_.*_bar$", "complex": { "a": 1 } })

        var params = matchParams
        params.list = "c"
        verify(!IntentServer.applicationIntent("match", "intents1", params))
        params.list = "b"
        verify(IntentServer.applicationIntent("match", "intents1", params))

        params.int = 2
        verify(!IntentServer.applicationIntent("match", "intents1", params))
        params.int = 42

        params.string = "foo"
        verify(!IntentServer.applicationIntent("match", "intents1", params))
        params.string = "foo_test_bar"
        verify(IntentServer.applicationIntent("match", "intents1", params))

        params.complex = "string"
        verify(!IntentServer.applicationIntent("match", "intents1", params))
        params.complex = matchParams.complex
    }


    function test_intents_data() {
        return [
                    {tag: "1-1", intentId: "only1", appId: "intents1", succeeding: true },
                    {tag: "2-2", intentId: "only2", appId: "intents2.1", succeeding: true },
                    {tag: "1-2", intentId: "only1", appId: "intents2.1", succeeding: false,
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
                        errorMessage: /Starting handler application timed out after .*/, isTimeout: true },
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
        let requestTimeout = spyTimeout
        if (data.isTimeout)
            requestTimeout *= 10
        tryCompare(requestSpy, "count", 1, requestTimeout)
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
            tryCompare(disambiguateSpy, "count", 1, spyTimeout)
            var possibleIntents = disambiguateSpy.signalArguments[0][1]
            compare(possibleIntents.length, 2)
            compare(possibleIntents[0].intentId, intentId)
            compare(possibleIntents[1].intentId, intentId)
            compare(possibleIntents[0].applicationId, "intents1")
            compare(possibleIntents[1].applicationId, "intents2.1")
            compare(disambiguateSpy.signalArguments[0][2], stdParams)

            switch (data.action) {
            case "reject":
                IntentServer.rejectDisambiguationRequest(disambiguateSpy.signalArguments[0][0])
                break
            case "acknowledge":
                var intent = data.acknowledgeIntentId ? IntentServer.applicationIntent(data.acknowledgeIntentId,
                                                                                possibleIntents[0].applicationId)
                                                      : possibleIntents[1]
                IntentServer.acknowledgeDisambiguationRequest(disambiguateSpy.signalArguments[0][0], intent)
                break
            }
            disambiguateSpy.clear()
        }

        tryCompare(requestSpy, "count", 1, spyTimeout * (data.action === "timeout" ? 15 : 1))
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

    IntentServerHandler {
        id: broadcastReceiver
        intentIds: [ "broadcast/pong" ]
        visibility: IntentObject.Public
        onRequestReceived: (request) => {
            pongsReceived.push(request.parameters.from)
        }
        property var pongsReceived: []
    }


    function test_zbroadcast() { // 'z' to make it run as the last test
        let am = ApplicationManager

        // stop all running applications
        am.stopAllApplications(true)
        tryVerify(() => { return am.applicationRunState("intents1") === Am.NotRunning })
        tryVerify(() => { return am.applicationRunState("intents2.1") === Am.NotRunning })

        broadcastReceiver.pongsReceived = []

        // broadcast ping -> only intents1 should be auto-started
        verify(IntentClient.broadcastIntentRequest("broadcast/ping", { }))

        tryVerify(() => { return am.applicationRunState("intents1") === Am.Running })
        tryVerify(() => { return am.applicationRunState("intents2.1") === Am.NotRunning })

        tryVerify(() => { return broadcastReceiver.pongsReceived.join() === "intents1"})

        // manually start intents2.1 and ping again -> both apps should receive it

        verify(am.startApplication("intents2.1"))
        tryVerify(() => { return am.applicationRunState("intents2.1") === Am.Running })
        wait(250 * AmTest.timeoutFactor) // it takes a bit for the intents IPC to get initialized

        broadcastReceiver.pongsReceived = []

        verify(IntentClient.broadcastIntentRequest("broadcast/ping", { }))

        tryVerify(() => { return broadcastReceiver.pongsReceived.sort().join() === "intents1,intents2" })
    }
}
