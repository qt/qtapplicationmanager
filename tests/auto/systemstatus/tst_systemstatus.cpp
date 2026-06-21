// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtCore>
#include <QtTest>

#if !defined(Q_OS_LINUX)
#  error "This test is Linux specific!"
#endif

#include <unistd.h>

#include <cpustatus.h>
#include <memorystatus.h>
#include <iostatus.h>
#include <cgroupstatus.h>
#include <pressurestallinformation.h>
#include <systemstatus.h>
#include <utilities.h>


using namespace Qt::StringLiterals;

QT_USE_NAMESPACE_AM

class tst_SystemStatus : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cpuStatus();
    void memoryStatus();
    void ioStatus();
    void cgroupStatus();
    void pressureStallInformation();
    void systemStatus();

};


void tst_SystemStatus::initTestCase()
{
#if !defined(QT_BUILD_INTERNAL)
    QSKIP("Not a developer build");
#else
    setTestRootPathPrefix(u":/root/"_s);
#endif
}

void tst_SystemStatus::cpuStatus()
{
    // Parse the first line of /proc/stat: "cpu  user nice system idle ..."
    // CpuStatus reads the first 4 numbers, sums them as total, treats index 3 as idle.
    QFile f(u":/root/proc/stat"_s);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const auto parts = f.readLine().simplified().split(' ');
    QVERIFY(parts.size() >= 5 && parts[0] == "cpu");
    const qint64 idle  = parts[4].toLongLong();
    const qint64 total = parts[1].toLongLong() + parts[2].toLongLong()
                         + parts[3].toLongLong() + idle;

    CpuStatus cpu;
    // Constructor read: lastIdle=0, lastTotal=0 -> load = 1 - idle/total
    QCOMPARE(cpu.cpuLoad(), qreal(1) - qreal(idle) / qreal(total));
    QVERIFY(cpu.cpuCores() > 0);

    // update() re-reads the same static file -> load calculation fails, because time did not advance
    cpu.update();
    QCOMPARE(cpu.cpuLoad(), 0);
}

void tst_SystemStatus::memoryStatus()
{
    // Parse MemAvailable from /proc/meminfo (value in kB)
    QFile f(u":/root/proc/meminfo"_s);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray buffer = f.readAll();

    static constexpr char key[] = "MemAvailable: ";
    const qsizetype idx = buffer.indexOf(key);
    QVERIFY(idx >= 0);
    const quint64 memAvailableKiB = ::strtoull(buffer.constData() + idx + sizeof(key) - 1, nullptr, 10);

    MemoryStatus mem;
    QVERIFY(mem.totalMemory() > 0);
    QCOMPARE(mem.memoryUsed(), mem.totalMemory() - 1024ULL * memAvailableKiB);

    // update() re-reads the same static file -> result must be stable
    mem.update();
    QCOMPARE(mem.memoryUsed(), mem.totalMemory() - 1024ULL * memAvailableKiB);
}

void tst_SystemStatus::ioStatus()
{
    // Parse field index 9 (time doing I/Os, in ms) from /sys/block/nvme0n1/stat.
    // IoReader uses the same digit-scanning loop and reads exactly 11 values.
    QFile f(u":/root/sys/block/nvme0n1/stat"_s);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray buf = f.readAll();
    qsizetype pos = 0;
    int fieldCount = 0;
    while (pos < buf.size() && fieldCount < 11) {
        if (!::isdigit(buf.at(pos))) {
            ++pos;
            continue;
        }
        char *end = nullptr;
        ::strtoll(buf.constData() + pos, &end, 10);
        ++fieldCount;
        pos = qsizetype(end - buf.constData() + 1);
    }
    QVERIFY(fieldCount >= 11);

    // root/dev/nvme0n1 must exist so that addIoReader() doesn't bail out
    QVERIFY(QFile::exists(u":/root/dev/nvme0n1"_s));

    IoStatus io;
    io.setDeviceNames({ u"nvme0n1"_s });

    // First update(): constructor primed the timer (m_lastIoTime=0), so load = ioTime/elapsed.
    // Elapsed is real wall time, so we can only check the value is non-negative.
    io.update();
    QVERIFY(io.ioLoad().contains(u"nvme0n1"_s));
    QVERIFY(io.ioLoad().value(u"nvme0n1"_s).toReal() >= 0);

    // Second update(): same static file, ioTime unchanged -> load = 0
    io.update();
    QCOMPARE(io.ioLoad().value(u"nvme0n1"_s).toReal(), 0.0);
}

void tst_SystemStatus::cgroupStatus()
{
    // Parse the expected group path from /proc/self/cgroup: "0::<path>"
    QFile cgroupFile(u":/root/proc/self/cgroup"_s);
    QVERIFY(cgroupFile.open(QIODevice::ReadOnly));
    const QByteArray cgroupLine = cgroupFile.readLine().trimmed();
    QVERIFY(cgroupLine.startsWith("0::/"));
    const QString expectedPath = QString::fromLocal8Bit(cgroupLine.mid(4));

    // Mirror readCGroupValue(): "max" maps to the sentinel, otherwise parse as bytes
    const QString base = u":/root/sys/fs/cgroup/"_s + expectedPath + u'/';
    auto readCGroupValue = [](const QString &path) -> quint64 {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return 0;
        const QByteArray data = f.readLine().trimmed();
        if (data == "max")
            return std::numeric_limits<quint64>::max();
        bool ok;
        const quint64 value = data.toULongLong(&ok);
        return ok ? value : 0;
    };
    const quint64 expectedMemoryHigh = readCGroupValue(base + u"memory.high"_s);
    const quint64 expectedMemoryMax  = readCGroupValue(base + u"memory.max"_s);

    QFile memCurrentFile(base + u"memory.current"_s);
    QVERIFY(memCurrentFile.open(QIODevice::ReadOnly));
    const quint64 expectedMemoryUsed = ::strtoull(memCurrentFile.readAll().constData(), nullptr, 10);

    CGroupStatus cg;
    QCOMPARE(cg.path(), expectedPath);
    QCOMPARE(cg.memoryHigh(), expectedMemoryHigh);
    QCOMPARE(cg.memoryMax(), expectedMemoryMax);
    QCOMPARE(cg.memoryUsed(), expectedMemoryUsed);

    // update() re-reads the same static file -> result must be stable
    cg.update();
    QCOMPARE(cg.memoryUsed(), expectedMemoryUsed);
}

void tst_SystemStatus::pressureStallInformation()
{
    // pure property logic - no syscalls, always runs.
    {
        PressureStallInformation psi(PressureStallInformation::Type::Memory, nullptr);
        QCOMPARE(psi.type(), PressureStallInformation::Type::Memory);
        QCOMPARE(psi.mode(), PressureStallInformation::Mode::Off);
        QCOMPARE(psi.timeWindow(), 0u);
        QCOMPARE(psi.stallTime(), 0u);
        QVERIFY(!psi.isActive());

        QSignalSpy modeSpy(&psi, &PressureStallInformation::modeChanged);
        QSignalSpy windowSpy(&psi, &PressureStallInformation::timeWindowChanged);
        QSignalSpy stallSpy(&psi, &PressureStallInformation::stallTimeChanged);

        psi.setTimeWindow(2000);
        QCOMPARE(psi.timeWindow(), 2000u);
        QCOMPARE(windowSpy.count(), 1);
        psi.setTimeWindow(2000); // idempotent
        QCOMPARE(windowSpy.count(), 1);

        psi.setStallTime(50);
        QCOMPARE(psi.stallTime(), 50u);
        QCOMPARE(stallSpy.count(), 1);
        psi.setStallTime(50); // idempotent
        QCOMPARE(stallSpy.count(), 1);

        // enabling a mode without a pressure file set: reconfigure runs but cannot arm anything
        psi.setMode(PressureStallInformation::Mode::Some);
        QCOMPARE(psi.mode(), PressureStallInformation::Mode::Some);
        QCOMPARE(modeSpy.count(), 1);
        psi.setMode(PressureStallInformation::Mode::Some); // idempotent
        QCOMPARE(modeSpy.count(), 1);

        QTest::qWait(100); // let the (empty-file) reconfigure run; it must stay inactive
        QVERIFY(!psi.isActive());
    }

    // error path - arming against a non-existent file must fail and stay inactive.
    {
        PressureStallInformation psi(PressureStallInformation::Type::Cpu, nullptr);
        QSignalSpy activeSpy(&psi, &PressureStallInformation::activeChanged);

        psi.setPressureFile(u"/this/path/does/not/exist/pressure"_s);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"Failed to open PSI fd.*"_s));
        psi.setMode(PressureStallInformation::Mode::Some);

        // let the debounced reconfigure timer fire; it must not arm the trigger
        QTest::qWait(100);
        QVERIFY(!psi.isActive());
        QCOMPARE(activeSpy.count(), 0); // never went active, so no transition
    }

    // the remaining checks need a real PSI file - only available on a PSI-enabled kernel.
    // there might also be AppArmor restrictions (Ubuntu 22.04)
    {
        QFile f(u"/proc/pressure/memory"_s);
        if (!f.exists() || !f.open(QIODevice::WriteOnly))
            QSKIP("System-wide PSI for users is not available on this kernel");
    }

    // happy path - configure a legal trigger and verify it actually arms.
    // Use the Memory type to exercise a different branch of the type-string mapping.
    {
        PressureStallInformation psi(PressureStallInformation::Type::Memory, nullptr);
        QSignalSpy activeSpy(&psi, &PressureStallInformation::activeChanged);

        psi.setPressureFile(u"/proc/pressure/memory"_s);
        psi.setStallTime(50);
        psi.setTimeWindow(2000); // legal even for unprivileged users (multiple of 2000ms)
        psi.setMode(PressureStallInformation::Mode::Some);

        QTRY_VERIFY(psi.isActive());
        QCOMPARE(activeSpy.count(), 1);

        // re-pointing the pressure file while active re-arms the trigger on the new file
        psi.setPressureFile(u"/proc/pressure/cpu"_s);
        QTRY_VERIFY(psi.isActive());

        // setting mode back to Off must tear the trigger down again
        psi.setMode(PressureStallInformation::Mode::Off);
        QTRY_VERIFY(!psi.isActive());
    }

    // EINVAL path - unprivileged users can't set a window that isn't a multiple of 2000ms.
    // Use the Io type to exercise the remaining branch of the type-string mapping.
    if (::geteuid() != 0) {
        PressureStallInformation psi(PressureStallInformation::Type::Io, nullptr);

        psi.setPressureFile(u"/proc/pressure/io"_s);
        psi.setStallTime(50);
        psi.setTimeWindow(1500); // illegal for unprivileged users
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"Failed to write PSI trigger data.*"_s));
        psi.setMode(PressureStallInformation::Mode::Some);

        QTest::qWait(100);
        QVERIFY(!psi.isActive());
    }
}

void tst_SystemStatus::systemStatus()
{
    // Reparse the same fixtures used by cpuStatus() and memoryStatus() - SystemStatus
    // composes CpuStatus and MemoryStatus internally, so it must agree with them.
    QFile cpuFile(u":/root/proc/stat"_s);
    QVERIFY(cpuFile.open(QIODevice::ReadOnly));
    const auto parts = cpuFile.readLine().simplified().split(' ');
    QVERIFY(parts.size() >= 5 && parts[0] == "cpu");
    const qint64 idle  = parts[4].toLongLong();
    const qint64 total = parts[1].toLongLong() + parts[2].toLongLong()
                         + parts[3].toLongLong() + idle;

    QFile memFile(u":/root/proc/meminfo"_s);
    QVERIFY(memFile.open(QIODevice::ReadOnly));
    const QByteArray memBuffer = memFile.readAll();
    static constexpr char memKey[] = "MemAvailable: ";
    const qsizetype memIdx = memBuffer.indexOf(memKey);
    QVERIFY(memIdx >= 0);
    const quint64 memAvailableKiB = ::strtoull(memBuffer.constData() + memIdx + sizeof(memKey) - 1,
                                               nullptr, 10);

    SystemStatus sys;

    // Memory: SystemStatus must agree with MemoryStatus on totalMemory() / memoryUsed().
    QVERIFY(sys.memoryMax() > 0);
    QCOMPARE(sys.memoryUsed(), sys.memoryMax() - 1024ULL * memAvailableKiB);

    // CPU: SystemStatus must agree with CpuStatus on the constructor's initial read.
    QCOMPARE(sys.cpuLoad(), qreal(1) - qreal(idle) / qreal(total));
    QVERIFY(sys.cpuCores() > 0);

    // PSI children must exist, expose the right Type, and start Off.
    QVERIFY(sys.cpuPSI() != nullptr);
    QVERIFY(sys.memoryPSI() != nullptr);
    QVERIFY(sys.ioPSI() != nullptr);
    QCOMPARE(sys.cpuPSI()->type(), PressureStallInformation::Type::Cpu);
    QCOMPARE(sys.memoryPSI()->type(), PressureStallInformation::Type::Memory);
    QCOMPARE(sys.ioPSI()->type(), PressureStallInformation::Type::Io);
    QCOMPARE(sys.cpuPSI()->mode(), PressureStallInformation::Mode::Off);
    QCOMPARE(sys.memoryPSI()->mode(), PressureStallInformation::Mode::Off);
    QCOMPARE(sys.ioPSI()->mode(), PressureStallInformation::Mode::Off);

    // update() re-reads the same static files: memoryUsed is stable, cpuLoad drops to 0
    // (CpuStatus returns 0 when total is unchanged between reads - see cpuStatus()).
    sys.update();
    QCOMPARE(sys.memoryUsed(), sys.memoryMax() - 1024ULL * memAvailableKiB);
    QCOMPARE(sys.cpuLoad(), 0);
}

QTEST_GUILESS_MAIN(tst_SystemStatus)

#include "tst_systemstatus.moc"
