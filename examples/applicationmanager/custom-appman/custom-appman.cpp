// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QtAppManCommon/global.h>
#include <QtAppManCommon/logging.h>
#include <QtAppManMain/main.h>
#include <QtAppManMain/configuration.h>
#include <QtAppManManager/sudo.h>


QT_USE_NAMESPACE_AM

Q_DECL_EXPORT int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(QStringLiteral("Custom Application Manager"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    Logging::initialize(argc, argv);
    Sudo::forkServer(Sudo::DropPrivilegesPermanently);

    std::unique_ptr<Main> a;
    std::unique_ptr<Configuration> cfg;

    try {
        a = std::make_unique<Main>(argc, argv);
        cfg = std::make_unique<Configuration>();
        cfg->parseWithArguments(QCoreApplication::arguments());

        a->setup(cfg.get());
        a->loadQml(cfg->loadDummyData());
        a->showWindow(cfg->fullscreen() && !cfg->noFullscreen());
    } catch (const std::exception &e) {
        qCCritical(LogSystem) << "ERROR:" << e.what();
        return 2;
    }
    return MainBase::exec();
}
