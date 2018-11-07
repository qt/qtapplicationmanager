/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#pragma once

#include <QObject>
#include <QtQml/qqmlpropertymap.h>
#include <QPointer>

#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class QTestRootObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool windowShown READ windowShown NOTIFY windowShownChanged)
    Q_PROPERTY(bool hasTestCase READ hasTestCase WRITE setHasTestCase NOTIFY hasTestCaseChanged)
    Q_PROPERTY(QObject *defined READ defined)

public:
    QTestRootObject(QObject *parent = nullptr);

    static QTestRootObject *instance();

    bool hasQuit() const { return m_hasQuit; }

    bool hasTestCase() const { return m_hasTestCase; }
    void setHasTestCase(bool value) { m_hasTestCase = value; emit hasTestCaseChanged(); }

    bool windowShown() const { return m_windowShown; }
    void setWindowShown(bool value) { m_windowShown = value; emit windowShownChanged(); }
    QQmlPropertyMap *defined() const { return m_defined; }

    void init() { setWindowShown(false); setHasTestCase(false); m_hasQuit = false; }

Q_SIGNALS:
    void windowShownChanged();
    void hasTestCaseChanged();

private Q_SLOTS:
    void quit() { m_hasQuit = true; }

private:
    bool m_windowShown;
    bool m_hasTestCase;
    bool m_hasQuit;
    QQmlPropertyMap *m_defined;
    friend class TestRunner;
};

class AmTest : public QObject
{
    Q_OBJECT

    AmTest();

public:
    enum MsgType { DebugMsg, WarningMsg, CriticalMsg, FatalMsg, InfoMsg, SystemMsg = CriticalMsg };
    Q_ENUM(MsgType)

    static AmTest *instance();

    Q_INVOKABLE void ignoreMessage(MsgType type, const char* msg);
    Q_INVOKABLE void ignoreMessage(MsgType type, const QRegExp &expression);
};

QT_END_NAMESPACE_AM
