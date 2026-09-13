// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtCore/qglobal.h>

#if QT_VERSION < QT_VERSION_CHECK(6, 12, 0)
#  include "runtimeinterface_adaptor.h"
#else
#  include "runtimeinterface_adaptor_p.h"
#endif


QT_USE_NAMESPACE_AM

RuntimeInterfaceAdaptor::RuntimeInterfaceAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{ }

RuntimeInterfaceAdaptor::~RuntimeInterfaceAdaptor()
{ }
