// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef CACERTIFICATE_H
#define CACERTIFICATE_H

#include <QtCore/QString>
#include <QtAppManSystemUI/qtappmansystemuiglobal.h>

QT_FORWARD_DECLARE_CLASS(QDataStream)

QT_BEGIN_NAMESPACE_AM

struct Q_APPMANSYSTEMUI_EXPORT CaCertificate
{
public:
    QString file;
    enum class Scope { Common, Developer, Store } scope = Scope::Common;
    enum class Role { Any, Issuer, Intermediate, Root } role = Role::Any;

    CaCertificate() = default;
    CaCertificate(const CaCertificate &copy) = default;
    CaCertificate &operator=(const CaCertificate &other) = default;
    explicit CaCertificate(const QString &file, Scope scope = Scope::Common, Role role = Role::Any);

    bool operator==(const CaCertificate &other) const;
    bool operator!=(const CaCertificate &other) const;
};

// QDataStream operators
Q_APPMANSYSTEMUI_EXPORT QDataStream &operator<<(QDataStream &ds, const CaCertificate &cac);
Q_APPMANSYSTEMUI_EXPORT QDataStream &operator>>(QDataStream &ds, CaCertificate &cac);

QT_END_NAMESPACE_AM

#endif // CACERTIFICATE_H
