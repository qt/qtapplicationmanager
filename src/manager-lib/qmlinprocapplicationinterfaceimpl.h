// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef QMLINPROCAPPLICATIONINTERFACEIMPL_H
#define QMLINPROCAPPLICATIONINTERFACEIMPL_H

#include <QtCore/QVector>
#include <QtCore/QPointer>
#include <QtQml/QQmlParserStatus>

#include <QtAppManSharedMain/applicationinterfaceimpl.h>

QT_BEGIN_NAMESPACE_AM

class QmlInProcRuntime;
class Notification;

class QmlInProcApplicationInterfaceImpl : public ApplicationInterfaceImpl
{
public:
    explicit QmlInProcApplicationInterfaceImpl(ApplicationInterface *ai,
                                                  QmlInProcRuntime *runtime);

    QString applicationId() const override;
    QVariantMap name() const override;
    QUrl icon() const override;
    QString version() const override;
    QVariantMap systemProperties() const override;
    QVariantMap applicationProperties() const override;
    void acknowledgeQuit() override;

private:
    QmlInProcRuntime *m_runtime;
    friend class QmlInProcRuntime;

    Q_DISABLE_COPY_MOVE(QmlInProcApplicationInterfaceImpl)
};

QT_END_NAMESPACE_AM

#endif // QMLINPROCAPPLICATIONINTERFACEIMPL_H
