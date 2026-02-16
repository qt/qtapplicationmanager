// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "cacertificate.h"

#include <QDataStream>

QT_BEGIN_NAMESPACE_AM

CaCertificate::CaCertificate(const QString &_file, Scope _scope, Role _role)
    : file(_file)
    , scope(_scope)
    , role(_role)
{ }

bool CaCertificate::operator==(const CaCertificate &other) const
{
    return file == other.file
        && scope == other.scope
        && role == other.role;
}

bool CaCertificate::operator!=(const CaCertificate &other) const
{
    return !(*this == other);
}

QDataStream &operator<<(QDataStream &ds, const CaCertificate &cac)
{
    ds << cac.file << quint32(cac.scope) << quint32(cac.role);
    return ds;
}

QDataStream &operator>>(QDataStream &ds, CaCertificate &cac)
{
    quint32 scope;
    quint32 role;
    ds >> cac.file >> scope >> role;
    if ((scope < quint32(CaCertificate::Scope::Common)) || (scope > quint32(CaCertificate::Scope::Store)))
        scope = quint32(CaCertificate::Scope::Common);
    if ((role < quint32(CaCertificate::Role::Any)) || (role > quint32(CaCertificate::Role::Root)))
        role = quint32(CaCertificate::Role::Any);
    cac.scope = CaCertificate::Scope(scope);
    cac.role = CaCertificate::Role(role);
    return ds;
}

QT_END_NAMESPACE_AM
