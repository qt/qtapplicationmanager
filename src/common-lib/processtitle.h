// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef PROCESSTITLE_H
#define PROCESSTITLE_H

#include <QtAppManCommon/global.h>
#include <QtCore/QByteArrayView>

QT_BEGIN_NAMESPACE_AM

namespace ProcessTitle {

void setTitle(QByteArrayView title);
const char *title();

}

QT_END_NAMESPACE_AM

#endif // PROCESSTITLE_H
