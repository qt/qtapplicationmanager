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

#include <QQuickView>
#include <QtCore>
#include <QtTest>

#include "windowmanager.h"
#include "systemmonitor.h"
#include <QtAppManMonitor/private/systemmonitor_p.h>

QT_USE_NAMESPACE_AM

class tst_SystemMonitor : public QObject
{
    Q_OBJECT

public:
    tst_SystemMonitor();
    virtual ~tst_SystemMonitor();

private slots:
    void reportingTimer_data();
    void reportingTimer();
private:
    static bool getBooleanProperty(QObject *obj, QString propName);
    static QMetaProperty getProperty(QObject *obj, QString propName);
    static void setBooleanProperty(QObject *obj, QString propName, bool value);
    QQuickView *view;
};

tst_SystemMonitor::tst_SystemMonitor()
{
    view = new QQuickView;
    WindowManager::createInstance(view->engine(), QString("foo"));
}

tst_SystemMonitor::~tst_SystemMonitor()
{
    delete view;
}

QMetaProperty tst_SystemMonitor::getProperty(QObject *obj, QString propName)
{
    const QMetaObject *metaObj = obj->metaObject();
    int idx = metaObj->indexOfProperty(propName.toLocal8Bit());
    Q_ASSERT(idx != -1);

    return metaObj->property(idx);
}

bool tst_SystemMonitor::getBooleanProperty(QObject *obj, QString propName)
{
    QMetaProperty property = getProperty(obj, propName);
    QVariant result = property.read(obj);
    Q_ASSERT(result.type() == QVariant::Bool);
    return result.toBool();
}

void tst_SystemMonitor::setBooleanProperty(QObject *obj, QString propName, bool value)
{
    QMetaProperty property = getProperty(obj, propName);
    bool ok = property.write(obj, QVariant(value));
    Q_ASSERT(ok);
}

void tst_SystemMonitor::reportingTimer_data()
{
    QTest::addColumn<QString>("reportingEnabled");

    QTest::newRow("memory") << QString("memoryReportingEnabled");
    QTest::newRow("cpuLoad") << QString("cpuLoadReportingEnabled");
    QTest::newRow("gpuLoad") << QString("gpuLoadReportingEnabled");
    QTest::newRow("fps") << QString("fpsReportingEnabled");
}

/*
    Tests that the reportingTimer is only active while there's at least
    one reporting category enabled.
 */
void tst_SystemMonitor::reportingTimer()
{
    QFETCH(QString, reportingEnabled);

    SystemMonitor sysMon;
    auto sysMonPriv = SystemMonitorPrivate::get(&sysMon);

    sysMon.setReportingInterval(50);

    QCOMPARE(getBooleanProperty(&sysMon, reportingEnabled), false);
    QCOMPARE(sysMonPriv->reportingTimerId, 0);

    setBooleanProperty(&sysMon, reportingEnabled, true);

    QVERIFY(sysMonPriv->reportingTimerId > 0);

    setBooleanProperty(&sysMon, reportingEnabled, false);

    QCOMPARE(sysMonPriv->reportingTimerId, 0);
}

QTEST_MAIN(tst_SystemMonitor)

#include "tst_systemmonitor.moc"
