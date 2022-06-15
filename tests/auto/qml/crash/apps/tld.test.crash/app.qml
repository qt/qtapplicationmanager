// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick 2.3
import QtApplicationManager.Application 2.0
import QmlCrash 2.0

ApplicationManagerWindow {
    function accessIllegalMemory()
    {
        QmlCrash.accessIllegalMemory();
    }

    Connections {
        target: ApplicationInterface
        function onOpenDocument(documentUrl) {
            switch (documentUrl) {
            case "illegalMemory": accessIllegalMemory(); break;
            case "illegalMemoryInThread": QmlCrash.accessIllegalMemoryInThread(); break;
            case "stackOverflow": QmlCrash.forceStackOverflow(); break;
            case "divideByZero": QmlCrash.divideByZero(); break;
            case "unhandledException": QmlCrash.throwUnhandledException(); break;
            case "abort": QmlCrash.abort(); break;
            case "raise": QmlCrash.raise(7); break;
            case "gracefully": QmlCrash.exitGracefully(); break;
            }
        }
    }
}
