// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QGuiApplication>
#include <QQuickView>

#include "waylandqtamclientextension_v2_p.h"

using namespace Qt::StringLiterals;

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    auto qtamExtension = new WaylandQtAMClientExtensionV2();
    QQuickView w;
    w.setGeometry(0, 0, 100, 100);
    w.show();

    qsizetype expectedSize = 0;

    const auto args = app.arguments();
    if (auto pos = args.indexOf(u"--start-argument"_s); pos >= 0)
        expectedSize = args.at(pos + 1).toInt();

    QObject::connect(qtamExtension, &WaylandQtAMClientExtensionV2::windowPropertyChanged,
            &app, [&](QWindow *, const QString &, const QVariant &) {
        auto allProperties = qtamExtension->windowProperties(&w);
        if (allProperties.size() == expectedSize)
            qtamExtension->setWindowProperty(&w, u"BACKCHANNEL"_s, allProperties);
    });

    return app.exec();
}
