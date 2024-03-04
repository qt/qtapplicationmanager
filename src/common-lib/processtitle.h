// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef PROCESSTITLE_H
#define PROCESSTITLE_H

#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

namespace ProcessTitle {

extern const char *placeholderArgument;

void setTitle(const char *fmt, ...) Q_ATTRIBUTE_FORMAT_PRINTF(1, 2);
void adjustArgumentCount(int &argc);
void augmentCommand(const char* extension);
const char *title();

}

QT_END_NAMESPACE_AM

#endif // PROCESSTITLE_H
