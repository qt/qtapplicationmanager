// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef PROCESSTITLE_H
#define PROCESSTITLE_H

#include <QtAppManCommon/qtappmancommonglobal.h>
#include <QtCore/QByteArrayView>

QT_BEGIN_NAMESPACE_AM

namespace ProcessTitle {

Q_APPMANCOMMON_EXPORT void setTitle(QByteArrayView title);
Q_APPMANCOMMON_EXPORT const char *title();

}

QT_END_NAMESPACE_AM

#endif // PROCESSTITLE_H
