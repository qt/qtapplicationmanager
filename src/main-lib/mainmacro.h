// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef MAINMACRO_H
#define MAINMACRO_H

#include <QtAppManMain/main.h>
#include <QtAppManMain/configuration.h>

QT_BEGIN_NAMESPACE_AM
// needed for syncqt
QT_END_NAMESPACE_AM

#define QT_AM_MAIN(...) \
Q_DECL_EXPORT int main(int argc, char *argv[]) \
{ \
    std::unique_ptr<QtAM::Main> a; \
    std::unique_ptr<QtAM::Configuration> cfg; \
\
    try { \
        a = std::make_unique<QtAM::Main>(argc, argv, QtAM::Main::InitFlag::ForkSudoServer \
                                                     | QtAM::Main::InitFlag::InitializeLogging); \
        /* this is deliberately not using make_unique to be able to use initializer lists */ \
        cfg.reset(new QtAM::Configuration(__VA_ARGS__)); \
        cfg->parseWithArguments(QCoreApplication::arguments()); \
\
        a->setup(cfg.get()); \
        a->loadQml(); \
        a->showWindow(); \
    } catch (const std::exception &e) { \
        qCritical() << "ERROR:" << e.what(); \
        return 2; \
    } \
\
    return QtAM::Main::exec(); \
}

#endif // MAINMACRO_H
