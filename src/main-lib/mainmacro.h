// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#pragma once

#include <QtAppManMain/main.h>
#include <QtAppManMain/configuration.h>

QT_BEGIN_NAMESPACE_AM
// needed for syncqt
QT_END_NAMESPACE_AM

#define QT_AM_MAIN() \
Q_DECL_EXPORT int main(int argc, char *argv[]) \
{ \
    try { \
        QtAM::Main a(argc, argv, QtAM::Main::InitFlag::ForkSudoServer | QtAM::Main::InitFlag::InitializeLogging); \
 \
        QtAM::Configuration cfg; \
        cfg.parseWithArguments(QCoreApplication::arguments()); \
 \
        a.setup(&cfg); \
        a.loadQml(cfg.loadDummyData()); \
        a.showWindow(cfg.fullscreen() && !cfg.noFullscreen()); \
 \
        return QtAM::MainBase::exec(); \
    } catch (const std::exception &e) { \
        qCritical() << "ERROR:" << e.what(); \
        return 2; \
    } \
}
