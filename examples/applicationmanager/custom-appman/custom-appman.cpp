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

using namespace Qt::StringLiterals;


QT_USE_NAMESPACE_AM

Q_DECL_EXPORT int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(u"Custom Application Manager"_s);
    QCoreApplication::setApplicationVersion(u"0.1"_s);

    std::unique_ptr<Main> a;
    std::unique_ptr<Configuration> cfg;

    try {
        a = std::make_unique<Main>(argc, argv, Main::InitFlag::ForkSudoServer
                                                   | Main::InitFlag::InitializeLogging);
        cfg = std::make_unique<Configuration>();
        cfg->parseWithArguments(QCoreApplication::arguments());

        a->setup(cfg.get());
        a->loadQml();
        a->showWindow();

    } catch (const std::exception &e) {
        qCritical() << "ERROR:" << e.what();
        return 2;
    }
    return Main::exec();
}

#endif
