// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#if 0

#include <QtAppManMain/mainmacro.h>
QT_AM_MAIN()

#else

#include <QtAppManMain/main.h>
#include <QtAppManMain/configuration.h>


QT_USE_NAMESPACE_AM

Q_DECL_EXPORT int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(QStringLiteral("Custom Application Manager"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    try {
        Main a(argc, argv, Main::InitFlag::ForkSudoServer | Main::InitFlag::InitializeLogging);

        Configuration cfg;
        cfg.parseWithArguments(QCoreApplication::arguments());

        a.setup(&cfg);
        a.loadQml(cfg.loadDummyData());
        a.showWindow(cfg.fullscreen() && !cfg.noFullscreen());

        return Main::exec();
    } catch (const std::exception &e) {
        qCritical() << "ERROR:" << e.what();
        return 2;
    }
}

#endif
