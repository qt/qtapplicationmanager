// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QLoggingCategory>
#include <QtAppManPluginInterfaces/startupinterface.h>

Q_DECLARE_LOGGING_CATEGORY(LogMe)

class TestStartupInterface : public QObject, public StartupInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID AM_StartupInterface_iid)
    Q_INTERFACES(StartupInterface)

public:
    TestStartupInterface();

    // StartupInterface
    void initialize(const QVariantMap &systemProperties) noexcept(false) override;
    void afterRuntimeRegistration() noexcept(false) override;

    void beforeQmlEngineLoad(QQmlEngine *engine) noexcept(false) override;
    void afterQmlEngineLoad(QQmlEngine *engine) noexcept(false) override;

    void beforeWindowShow(QWindow *window) noexcept(false) override;
    void afterWindowShow(QWindow *window) noexcept(false) override;
};
