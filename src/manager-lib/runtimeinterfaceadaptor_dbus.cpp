// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial

#include "runtimeinterface_adaptor.h"


QT_USE_NAMESPACE_AM

RuntimeInterfaceAdaptor::RuntimeInterfaceAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{ }

RuntimeInterfaceAdaptor::~RuntimeInterfaceAdaptor()
{ }
