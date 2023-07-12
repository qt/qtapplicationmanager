// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtAppManCommon/global.h>
#include <QtCore/QVariantMap>

QT_FORWARD_DECLARE_CLASS(QQmlEngine)

QT_BEGIN_NAMESPACE_AM

namespace Architecture {

QString identify(const QString &fileName);

}

QT_END_NAMESPACE_AM
