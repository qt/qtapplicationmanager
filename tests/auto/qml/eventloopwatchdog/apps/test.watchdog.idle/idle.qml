// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.Application

// An app with a responsive event loop: no event handler ever blocks, so the event-loop
// watchdog must never kill it.
ApplicationManagerWindow {
}
