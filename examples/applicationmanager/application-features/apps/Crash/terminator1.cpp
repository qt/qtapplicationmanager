// Copyright (C) 2023 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "terminator1.h"
#include <stdlib.h>


Terminator1::Terminator1(QObject *parent) : QObject(parent)
{
}

void Terminator1::abort() const
{
    ::abort();
}

void Terminator1::throwUnhandledException() const
{
    throw 42;
}

void Terminator1::exitGracefully() const
{
    exit(5);
}
