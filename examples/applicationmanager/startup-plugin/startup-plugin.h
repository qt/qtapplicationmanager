// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QLoggingCategory>
#include <QtAppManPluginInterfaces/startupinterface.h>
#include <QtAppManCommon/global.h>

Q_DECLARE_LOGGING_CATEGORY(LogMe)

class TestStartupInterface : public QObject, public StartupInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID AM_StartupInterface_iid)
    Q_INTERFACES(StartupInterface)

public:
    // StartupInterface
    void initialize(const QVariantMap &systemProperties) Q_DECL_NOEXCEPT_EXPR(false) override;
    void afterRuntimeRegistration() Q_DECL_NOEXCEPT_EXPR(false) override;

    void beforeQmlEngineLoad(QQmlEngine *engine) Q_DECL_NOEXCEPT_EXPR(false) override;
    void afterQmlEngineLoad(QQmlEngine *engine) Q_DECL_NOEXCEPT_EXPR(false) override;

    void beforeWindowShow(QWindow *window) Q_DECL_NOEXCEPT_EXPR(false) override;
    void afterWindowShow(QWindow *window) Q_DECL_NOEXCEPT_EXPR(false) override;
};
