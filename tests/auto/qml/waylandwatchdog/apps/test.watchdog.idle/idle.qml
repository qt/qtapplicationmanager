// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtApplicationManager.Application

// A completely idle client: after the first frame nothing keeps the scene dirty and no
// timer wakes the event loop. The Wayland event loop is still spinning in poll(), so the
// xdg-shell ping should still be answered automatically.
ApplicationManagerWindow {
}
