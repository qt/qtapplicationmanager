// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "intentclientsysteminterface.h"
#include "intentclient.h"
#include "intentclientrequest.h"

QT_BEGIN_NAMESPACE_AM

IntentClientSystemInterface::IntentClientSystemInterface(QObject *parent)
    : QObject(parent)
{ }

void IntentClientSystemInterface::initialize(IntentClient *intentClient)
{
    m_ic = intentClient;
    connect(this, &IntentClientSystemInterface::requestToSystemFinished,
            m_ic, &IntentClient::requestToSystemFinished);
    connect(this, &IntentClientSystemInterface::replyFromSystem,
            m_ic, &IntentClient::replyFromSystem);
    connect(this, &IntentClientSystemInterface::requestToApplication,
            m_ic, &IntentClient::requestToApplication);
}


QT_END_NAMESPACE_AM

#include "moc_intentclientsysteminterface.cpp"
