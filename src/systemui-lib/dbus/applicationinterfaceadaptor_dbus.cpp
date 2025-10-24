// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "applicationinterface_adaptor.h"
#include "applicationmanager.h"
#include "application.h"
#include "nativeruntime.h"


QT_USE_NAMESPACE_AM

static inline NativeRuntime *nativeRuntimeFor(ApplicationInterfaceAdaptor *adaptor)
{
    return (adaptor && adaptor->parent()) ? qobject_cast<NativeRuntime *>(adaptor->parent()->parent())
                                          : nullptr;
}

ApplicationInterfaceAdaptor::ApplicationInterfaceAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    auto *nr = nativeRuntimeFor(this);
    Q_ASSERT(nr);

    connect(ApplicationManager::instance(), &ApplicationManager::memoryLowWarning,
            this, &ApplicationInterfaceAdaptor::memoryLowWarning);
    connect(ApplicationManager::instance(), &ApplicationManager::memoryCriticalWarning,
            this, &ApplicationInterfaceAdaptor::memoryCriticalWarning);
    connect(nr->container(), &AbstractContainer::memoryLowWarning,
            this, &ApplicationInterfaceAdaptor::memoryLowWarning);
    connect(nr->container(), &AbstractContainer::memoryCriticalWarning,
            this, &ApplicationInterfaceAdaptor::memoryCriticalWarning);
    connect(nr, &NativeRuntime::aboutToStop,
            this, &ApplicationInterfaceAdaptor::quit);
}

ApplicationInterfaceAdaptor::~ApplicationInterfaceAdaptor()
{ }

void ApplicationInterfaceAdaptor::finishedInitialization()
{
    if (auto *nr = nativeRuntimeFor(this))
        nr->applicationFinishedInitialization();
}
