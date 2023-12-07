// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QStringList>
#include <QtCore/QVariantMap>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class Intent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString intentId READ intentId CONSTANT FINAL)
    Q_PROPERTY(QString packageId READ packageId CONSTANT FINAL)
    Q_PROPERTY(QString applicationId READ applicationId CONSTANT FINAL)
    Q_PROPERTY(QtAM::Intent::Visibility visibility READ visibility CONSTANT FINAL)
    Q_PROPERTY(QStringList requiredCapabilities READ requiredCapabilities CONSTANT FINAL)
    Q_PROPERTY(QVariantMap parameterMatch READ parameterMatch CONSTANT FINAL)

    Q_PROPERTY(QUrl icon READ icon CONSTANT FINAL)
    Q_PROPERTY(QString name READ name CONSTANT FINAL)
    Q_PROPERTY(QVariantMap names READ names CONSTANT FINAL)
    Q_PROPERTY(QString description READ description CONSTANT FINAL)
    Q_PROPERTY(QVariantMap descriptions READ descriptions CONSTANT FINAL)
    Q_PROPERTY(QStringList categories READ categories CONSTANT FINAL)

    Q_PROPERTY(bool handleOnlyWhenRunning READ handleOnlyWhenRunning CONSTANT REVISION(2, 1) FINAL)

public:
    enum Visibility {
        Public,
        Private
    };
    Q_ENUM(Visibility)

    Intent();

    QString intentId() const;
    Visibility visibility() const;
    QStringList requiredCapabilities() const;
    QVariantMap parameterMatch() const;

    QString packageId() const;
    QString applicationId() const;

    bool checkParameterMatch(const QVariantMap &parameters) const;

    QUrl icon() const;
    QString name() const;
    QVariantMap names() const;
    QString description() const;
    QVariantMap descriptions() const;
    QStringList categories() const;

    bool handleOnlyWhenRunning() const;

private:
    Intent(const QString &intentId, const QString &packageId, const QString &applicationId,
           const QStringList &capabilities, Intent::Visibility visibility,
           const QVariantMap &parameterMatch, const QMap<QString, QString> &names,
           const QMap<QString, QString> &descriptions, const QUrl &icon,
           const QStringList &categories, bool handleOnlyWhenRunning);

    Intent *copy() const; // needed during long-lived disambiguation requests

    QString m_intentId;
    Visibility m_visibility = Public;
    QStringList m_requiredCapabilities;
    QVariantMap m_parameterMatch;

    QString m_packageId;
    QString m_applicationId;

    QMap<QString, QString> m_names; // language -> name
    QMap<QString, QString> m_descriptions; // language -> description
    QStringList m_categories;
    QUrl m_icon;

    bool m_handleOnlyWhenRunning = false;

    friend class IntentServer;
    friend class IntentServerHandler;
    friend class IntentServerRequest;
    friend class TestPackageLoader; // for auto tests only
};

QT_END_NAMESPACE_AM

Q_DECLARE_METATYPE(QtAM::Intent *)
