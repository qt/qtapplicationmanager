// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "runtimeinterface_adaptor.h"


QT_USE_NAMESPACE_AM

RuntimeInterfaceAdaptor::RuntimeInterfaceAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{ }

RuntimeInterfaceAdaptor::~RuntimeInterfaceAdaptor()
{ }
