// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.4
import QtTest 1.0
import QtApplicationManager 2.1
import QtApplicationManager.SystemUI 2.1
import QtApplicationManager.Test

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
                        ignoreWarning: '-----------no-request-id------------ [only2] {:sysui: -> intents1} is an unknown intent' },
                    {tag: "match-1", intentId: "match", appId: "intents1", succeeding: true, params: matchParams },
                    {tag: "match-2", intentId: "match", appId: "intents1", succeeding: false,
                        params: function() { var x = Object.assign({}, matchParams); x.int = 1; return x }(),
                        errorMessage: "No matching intent handler registered.",
                        ignoreWarning: '-----------no-request-id------------ [match] {:sysui: -> intents1} is an unknown intent' },
                    {tag: "match-3", intentId: "match", appId: "intents1", succeeding: false, params: {},
                        errorMessage: "No matching intent handler registered.",
                        ignoreWarning: '-----------no-request-id------------ [match] {:sysui: -> intents1} is an unknown intent' },
                    {tag: "unknown-1", intentId: "unknown", appId: "intents1", succeeding: false,
                        errorMessage: "No matching intent handler registered.",
                        ignoreWarning: '-----------no-request-id------------ [unknown] {:sysui: -> intents1} is an unknown intent' },
                    {tag: "unknown-2", intentId: "unknown", appId: "", succeeding: false,
                        errorMessage: "No matching intent handler registered.",
                        ignoreWarning: '-----------no-request-id------------ [unknown] {:sysui: -> ?} is an unknown intent' },
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
                    {tag: "no-signal", action: "none", succeeding: false,
                        errorMessage: "Disambiguation required, but the System UI does not handle disambiguationRequest",
                        ignoreWarning: /.* \[both\] \{:sysui: -> \?\} requires disambiguation, but no receiver is connected to the disambiguationRequest signal/ },
                    {tag: "reject", action: "reject", succeeding: false,
                        errorMessage: "Disambiguation was rejected" },
                    {tag: "timeout", action: "timeout", succeeding: false,
                        errorMessage: /Disambiguation timed out after .*/ },
                    {tag: "ack-invalid", action: "acknowledge", acknowledgeIntentId: "only1", succeeding: false,
                        errorMessage: "Failed to disambiguate",
                        ignoreWarning: /.* \[both\] \{:sysui: -> \?\} disambiguated intent id \[only1\] is not valid/ },
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

    function test_intentRequestFilterFunction() {
        // sanity check: no filter set -> request passes
        compare(IntentServer.intentRequestFilterFunction, undefined)
        var req = IntentClient.sendIntentRequest("only1", "intents1", stdParams)
        verify(req)
        requestSpy.target = req
        tryCompare(requestSpy, "count", 1, spyTimeout)
        verify(req.succeeded)
        requestSpy.clear()
        requestSpy.target = null

        // filter that captures its arguments and returns true -> request passes
        var capturedIntents = null
        var capturedRequestingApp = null
        IntentServer.intentRequestFilterFunction = function(intents, requestingApplicationId) {
            capturedIntents = intents
            capturedRequestingApp = requestingApplicationId
            return true
        }

        req = IntentClient.sendIntentRequest("only1", "intents1", stdParams)
        verify(req)
        requestSpy.target = req
        tryCompare(requestSpy, "count", 1, spyTimeout)
        verify(req.succeeded)
        // the filter must have been invoked with exactly one candidate (only1@intents1)
        verify(capturedIntents)
        compare(capturedIntents.length, 1)
        compare(capturedIntents[0].intentId, "only1")
        compare(capturedIntents[0].applicationId, "intents1")
        compare(capturedRequestingApp, ":sysui:")
        requestSpy.clear()
        requestSpy.target = null

        // filter that returns false -> request fails
        IntentServer.intentRequestFilterFunction = function(intents, requestingApplicationId) {
            return false
        }
        ignoreWarning(/.* \[only1\] \{:sysui: -> intents1\} was rejected by the intentRequestFilterFunction/)

        req = IntentClient.sendIntentRequest("only1", "intents1", stdParams)
        verify(req)
        requestSpy.target = req
        tryCompare(requestSpy, "count", 1, spyTimeout)
        verify(!req.succeeded)
        compare(req.errorMessage, "No matching intent handler registered.")
        requestSpy.clear()
        requestSpy.target = null

        // ambiguous request: filter sees both candidates
        capturedIntents = null
        IntentServer.intentRequestFilterFunction = function(intents, requestingApplicationId) {
            capturedIntents = intents
            return true
        }
        disambiguateSpy.signalName = "disambiguationRequest"

        req = IntentClient.sendIntentRequest("both", stdParams)
        verify(req)
        requestSpy.target = req
        tryCompare(disambiguateSpy, "count", 1, spyTimeout)
        verify(capturedIntents)
        compare(capturedIntents.length, 2)
        // accept any candidate to let the request finish
        IntentServer.acknowledgeDisambiguationRequest(disambiguateSpy.signalArguments[0][0],
                                                     disambiguateSpy.signalArguments[0][1][0])
        tryCompare(requestSpy, "count", 1, spyTimeout)
        verify(req.succeeded)
        disambiguateSpy.clear()
        requestSpy.clear()
        requestSpy.target = null

        // clean up
        IntentServer.intentRequestFilterFunction = undefined
    }

    IntentModel {
        id: intentModel
    }

    SignalSpy {
        id: modelCountSpy
        target: intentModel
        signalName: "countChanged"
    }

    function test_model_count() {
        // with no filter the model mirrors the IntentServer source model
        compare(intentModel.filterFunction, undefined)
        compare(intentModel.sortFunction, undefined)
        compare(intentModel.count, IntentServer.count)
        verify(intentModel.count > 0)
    }

    function test_model_filter() {
        // how many intents the source model has for intents1
        var serverForApp = 0
        for (let s = 0; s < IntentServer.count; ++s) {
            if (IntentServer.intent(s).applicationId === "intents1")
                ++serverForApp
        }
        verify(serverForApp > 0)
        verify(serverForApp < IntentServer.count)

        // filter down to a single application's intents -> countChanged fires and count matches
        modelCountSpy.clear()
        intentModel.filterFunction = function(intent) { return intent.applicationId === "intents1" }
        tryCompare(intentModel, "count", serverForApp, spyTimeout)
        verify(modelCountSpy.count > 0)

        // a filter matching nothing yields an empty model
        intentModel.filterFunction = function(intent) { return intent.intentId === "does-not-exist" }
        tryCompare(intentModel, "count", 0, spyTimeout)

        // clearing the filter restores the full model
        intentModel.filterFunction = undefined
        tryCompare(intentModel, "count", IntentServer.count, spyTimeout)
    }

    function test_model_sort() {
        // sort ascending by intentId
        intentModel.sortFunction = function(a, b) { return a.intentId < b.intentId }
        compare(intentModel.count, IntentServer.count)
        for (let i = 1; i < intentModel.count; ++i) {
            let prev = IntentServer.intent(intentModel.mapToSource(i - 1)).intentId
            let cur = IntentServer.intent(intentModel.mapToSource(i)).intentId
            verify(prev <= cur)
        }

        // the reverse comparator produces the reverse order
        intentModel.sortFunction = function(a, b) { return a.intentId > b.intentId }
        for (let j = 1; j < intentModel.count; ++j) {
            let p = IntentServer.intent(intentModel.mapToSource(j - 1)).intentId
            let c = IntentServer.intent(intentModel.mapToSource(j)).intentId
            verify(p >= c)
        }

        intentModel.sortFunction = undefined
    }

    function test_model_indexOf() {
        intentModel.filterFunction = undefined
        intentModel.sortFunction = undefined

        // an intent present in the source maps to a valid model index and back
        var intent = IntentServer.applicationIntent("only1", "intents1")
        verify(intent)
        var idx = intentModel.indexOfIntent(intent)
        verify(idx >= 0)
        compare(intentModel.mapToSource(idx),
                IntentServer.indexOfIntent("only1", "intents1", {}))

        // the string-based overload resolves to the same index
        compare(intentModel.indexOfIntent("only1", "intents1", {}), idx)

        // an unknown intent is reported as not part of the model
        compare(intentModel.indexOfIntent("does-not-exist", "intents1", {}), -1)

        // once filtered out, the intent is no longer found in the model
        intentModel.filterFunction = function(i) { return i.applicationId === "intents2.1" }
        compare(intentModel.indexOfIntent(intent), -1)
        intentModel.filterFunction = undefined
    }

    function test_model_invalidate() {
        // invalidate() must not change a model with no filter/sort set
        intentModel.filterFunction = undefined
        intentModel.sortFunction = undefined
        var before = intentModel.count
        intentModel.invalidate()
        compare(intentModel.count, before)
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
        const timeout = 5000 * AmTest.timeoutFactor

        // stop all running applications
        am.stopAllApplications(true)
        tryVerify(() => { return am.applicationRunState("intents1") === Am.NotRunning }, timeout)
        tryVerify(() => { return am.applicationRunState("intents2.1") === Am.NotRunning }, timeout)

        broadcastReceiver.pongsReceived = []

        // broadcast ping -> only intents1 should be auto-started
        verify(IntentClient.broadcastIntentRequest("broadcast/ping", { }))

        tryVerify(() => { return am.applicationRunState("intents1") === Am.Running }, timeout)
        tryVerify(() => { return am.applicationRunState("intents2.1") === Am.NotRunning }, timeout)

        tryVerify(() => { return broadcastReceiver.pongsReceived.join() === "intents1"}, timeout)

        // manually start intents2.1 and ping again -> both apps should receive it

        verify(am.startApplication("intents2.1"))
        tryVerify(() => { return am.applicationRunState("intents2.1") === Am.Running }, timeout)
        wait(500 * AmTest.timeoutFactor) // it takes a bit for the intents IPC to get initialized

        broadcastReceiver.pongsReceived = []

        verify(IntentClient.broadcastIntentRequest("broadcast/ping", { }))

        tryVerify(() => { return broadcastReceiver.pongsReceived.sort().join() === "intents1,intents2" }, timeout)
    }
}
