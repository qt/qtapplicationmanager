/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore>
#include <QtTest>

#include "global.h"
#include "installationreport.h"

QT_USE_NAMESPACE_AM

class tst_InstallationReport : public QObject
{
    Q_OBJECT

public:
    tst_InstallationReport();

private slots:
    void test();
};

tst_InstallationReport::tst_InstallationReport()
{ }

void tst_InstallationReport::test()
{
    QStringList files { qSL("test"), qSL("more/test"), qSL("another/test/file") };

    InstallationReport ir(qSL("com.pelagicore.test"));
    QVERIFY(!ir.isValid());
    ir.addFile(files.first());
    QVERIFY(!ir.isValid());
    ir.setDiskSpaceUsed(42);
    QVERIFY(!ir.isValid());
    ir.setDigest("##digest##");
    QVERIFY(ir.isValid());
    ir.addFiles(files.mid(1));
    ir.setInstallationLocationId(qSL("test-42"));
    ir.setDeveloperSignature("%%dev-sig%%");
    ir.setStoreSignature("$$store-sig$$");

    QVERIFY(ir.isValid());
    QCOMPARE(ir.applicationId(), qSL("com.pelagicore.test"));
    QCOMPARE(ir.files(), files);
    QCOMPARE(ir.diskSpaceUsed(), 42ULL);
    QCOMPARE(ir.digest().constData(), "##digest##");
    QCOMPARE(ir.installationLocationId(), qSL("test-42"));
    QCOMPARE(ir.developerSignature().constData(), "%%dev-sig%%");
    QCOMPARE(ir.storeSignature().constData(), "$$store-sig$$");

    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    QVERIFY(ir.serialize(&buffer));
    buffer.seek(0);

    InstallationReport ir2;
    QVERIFY(ir2.deserialize(&buffer));
    buffer.seek(0);

    QVERIFY(ir2.isValid());
    QCOMPARE(ir2.applicationId(), qSL("com.pelagicore.test"));
    QCOMPARE(ir2.files(), files);
    QCOMPARE(ir2.diskSpaceUsed(), 42ULL);
    QCOMPARE(ir2.digest().constData(), "##digest##");
    QCOMPARE(ir2.installationLocationId(), qSL("test-42"));
    QCOMPARE(ir2.developerSignature().constData(), "%%dev-sig%%");
    QCOMPARE(ir2.storeSignature().constData(), "$$store-sig$$");

    QByteArray &yaml = buffer.buffer();
    QVERIFY(!yaml.isEmpty());

    int pos = yaml.lastIndexOf("\n---\nhmac: '");
    QVERIFY(pos > 0);
    pos += 12;
    QByteArray hmac = QMessageAuthenticationCode::hash("data", "key", QCryptographicHash::Sha256).toHex();
    yaml.replace(pos, hmac.size(), hmac);
    QCOMPARE(yaml.mid(pos + hmac.size(), 2).constData(), "'\n");

    QVERIFY(!ir2.deserialize(&buffer));
}

QTEST_APPLESS_MAIN(tst_InstallationReport)

#include "tst_installationreport.moc"
