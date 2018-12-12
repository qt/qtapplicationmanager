/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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

#include "processreader.h"

#include "logging.h"

#if defined(Q_OS_MACOS)
#  include <mach/mach.h>
#elif defined(Q_OS_LINUX)
#  include <unistd.h>
#endif

namespace {
    static uint parseValue(const char *pl) {
        while (*pl && (*pl < '0' || *pl > '9'))
            pl++;
        return static_cast<uint>(strtoul(pl, nullptr, 10));
    }
}

QT_USE_NAMESPACE_AM

void ProcessReader::setProcessId(qint64 pid)
{
    m_pid = pid;
    if (pid)
        openCpuLoad();
}

void ProcessReader::update()
{
    // read cpu
    {
        qreal cpuLoadFloat = readCpuLoad();
        quint32 value = ((qreal)std::numeric_limits<quint32>::max()) * cpuLoadFloat;
        cpuLoad.store(value);
    }

    {
        if (!readMemory()) {
            totalVm.store(0);
            totalRss.store(0);
            totalPss.store(0);
            textVm.store(0);
            textRss.store(0);
            textPss.store(0);
            heapVm.store(0);
            heapRss.store(0);
            heapPss.store(0);
        }
    }

    emit updated();
}

#if defined(Q_OS_LINUX)

void ProcessReader::openCpuLoad()
{
    const QByteArray fileName = "/proc/" + QByteArray::number(m_pid) + "/stat";
    m_statReader.reset(new SysFsReader(fileName));
    if (!m_statReader->isOpen())
        qCWarning(LogSystem) << "Cannot read CPU load from" << fileName;
}

qreal ProcessReader::readCpuLoad()
{
    qint64 elapsed;
    if (m_elapsedTime.isValid()) {
        elapsed = m_elapsedTime.restart();
    } else {
        elapsed = 0;
        m_elapsedTime.start();
    }

    if (m_statReader.isNull() || !m_statReader->isOpen()) {
        m_lastCpuUsage = 0.0;
        return 0.0;
    }

    QByteArray str = m_statReader->readValue();
    int pos = 0;
    int blanks = 0;
    while (pos < str.size() && blanks < 13) {
        if (isblank(str.at(pos)))
            ++blanks;
        ++pos;
    }

    char *endPtr = nullptr;
    quint64 utime = strtoull(str.constData() + pos, &endPtr, 10); // check missing for overflow
    pos = int(endPtr - str.constData() + 1);
    quint64 stime = strtoull(str.constData() + pos, nullptr, 10); // check missing for overflow

    qreal load = elapsed != 0 ? (utime + stime - m_lastCpuUsage) * 1000.0 / sysconf(_SC_CLK_TCK) / elapsed : 0.0;
    m_lastCpuUsage = utime + stime;
    return load;
}


bool ProcessReader::readMemory()
{
    QByteArray smapsFile = "/proc/" + QByteArray::number(m_pid) + "/smaps";
    return readSmaps(smapsFile);
}

bool ProcessReader::readSmaps(const QByteArray &smapsFile)
{
    quint32 _totalVm = 0;
    quint32 _totalRss = 0;
    quint32 _totalPss = 0;
    quint32 _textVm = 0;
    quint32 _textRss = 0;
    quint32 _textPss = 0;
    quint32 _heapVm = 0;
    quint32 _heapRss = 0;
    quint32 _heapPss = 0;

    struct ScopedFile {
        ~ScopedFile() { if (file) fclose(file); }
        FILE *file = nullptr;
    };

    ScopedFile sf;
    sf.file = fopen(smapsFile.constData(), "r");

    if (sf.file == nullptr)
        return false;

    const int lineLen = 100;  // we are not interested in full library paths
    char line[lineLen + 5];   // padding for highly unlikely trailing perm flags below
    char *pl;                 // pointer to chars within line
    bool ok = true;

    if (fgets(line, lineLen, sf.file) == nullptr)
        return false;

    // sanity checks
    pl = line;
    for (pl = line; pl < (line + 4) && ok; ++pl)
        ok = ((*pl >= '0' && *pl <= '9') || (*pl >= 'a' && *pl <= 'f'));
    while (strlen(line) == lineLen - 1 && line[lineLen - 2] != '\n') {
        if (Q_UNLIKELY(!fgets(line, lineLen, sf.file)))
            break;
    }
    if (fgets(line, lineLen, sf.file) == nullptr)
        return false;
    static const char strSize[] = "Size: ";
    ok = ok && !qstrncmp(line, strSize, sizeof(strSize) - 1);
    if (!ok)
        return false;

    // Determine block size
    ok = false;
    int blockLen = 0;
    while (fgets(line, lineLen, sf.file) != nullptr && !ok) {
        if (!(line[0] < '0' || line[0] > '9') && (line[0] < 'a' || line[0] > 'f'))
            ok = true;
        ++blockLen;
    }
    if (!ok || blockLen < 12 || blockLen > 32)
        return false;

    fseek(sf.file, 0, SEEK_SET);
    bool wasPrivateOnly = false;
    ok = false;

    while (true) {
        if (Q_UNLIKELY(!(fgets(line, lineLen, sf.file) != nullptr))) {
            ok = feof(sf.file);
            break;
        }

        // Determine permission flags
        pl = line;
        while (*pl && *pl != ' ')
            ++pl;
        char permissions[4];
        memcpy(permissions, ++pl, sizeof(permissions));

        // Determine inode
        int spaceCount = 0;
        while (*pl && spaceCount < 3) {
            if (*pl == ' ')
                ++spaceCount;
            ++pl;
        }
        bool hasInode = (*pl != '0');

        // Determine library name
        while (*pl && *pl != ' ')
            ++pl;
        while (*pl && *pl == ' ')
            ++pl;

        static const char strStack[] = "stack]";
        bool isMainStack = (Q_UNLIKELY(*pl == '['
                            && !qstrncmp(pl + 1, strStack, sizeof(strStack) - 1)));
        // Skip rest of library path
        while (strlen(line) == lineLen - 1 && line[lineLen - 2] != '\n') {
            if (Q_UNLIKELY(!fgets(line, lineLen, sf.file)))
                break;
        }

        int skipLen = blockLen;
        uint vm = 0;
        uint rss = 0;
        uint pss = 0;
        const int sizeTag = 0x01;
        const int rssTag  = 0x02;
        const int pssTag  = 0x04;
        const int allTags = sizeTag | rssTag | pssTag;
        int foundTags = 0;

        while (foundTags < allTags && skipLen > 0) {
            skipLen--;
            if (Q_UNLIKELY(!fgets(line, lineLen, sf.file)))
                break;
            pl = line;

            static const char strSize[] = "ize:";
            static const char strXss[] = "ss:";

            switch (*pl) {
            case 'S':
                if (!qstrncmp(pl + 1, strSize, sizeof(strSize) - 1)) {
                    foundTags |= sizeTag;
                    vm = parseValue(pl + sizeof(strSize));
                }
                break;
            case 'R':
                if (!qstrncmp(pl + 1, strXss, sizeof(strXss) - 1)) {
                    foundTags |= rssTag;
                    rss = parseValue(pl + sizeof(strXss));
                }
                break;
            case 'P':
                if (!qstrncmp(pl + 1, strXss, sizeof(strXss) - 1)) {
                    foundTags |= pssTag;
                    pss = parseValue(pl + sizeof(strXss));
                }
                break;
            }
        }

        if (foundTags < allTags)
            break;

        _totalVm += vm;
        _totalRss += rss;
        _totalPss += pss;

        static const char permRXP[] = { 'r', '-', 'x', 'p' };
        static const char permRWP[] = { 'r', 'w', '-', 'p' };
        if (!memcmp(permissions, permRXP, sizeof(permissions))) {
            _textVm += vm;
            _textRss += rss;
            _textPss += pss;
        } else if (!memcmp(permissions, permRWP, sizeof(permissions))
                   && !isMainStack && (vm != 8192 || hasInode || !wasPrivateOnly) // try to exclude stack
                   && !hasInode) {
            _heapVm += vm;
            _heapRss += rss;
            _heapPss += pss;
        }

        static const char permP[] = { '-', '-', '-', 'p' };
        wasPrivateOnly = !memcmp(permissions, permP, sizeof(permissions));

        for (int skip = skipLen; skip; --skip) {
            if (Q_UNLIKELY(!fgets(line, lineLen, sf.file)))
                break;
        }
    }

    if (ok) {
        // publish the readings
        totalVm.store(_totalVm);
        totalRss.store(_totalRss);
        totalPss.store(_totalPss);
        textVm.store(_textVm);
        textRss.store(_textRss);
        textPss.store(_textPss);
        heapVm.store(_heapVm);
        heapRss.store(_heapRss);
        heapPss.store(_heapPss);
    }

    return ok;
}

#elif defined(Q_OS_MACOS)

void ProcessReader::openCpuLoad()
{
}

qreal ProcessReader::readCpuLoad()
{
    return 0.0;
}

bool ProcessReader::readMemory()
{
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count)) {
        qCWarning(LogSystem) << "Could not read memory data";
        return false;
    }

    totalRss.store(t_info.resident_size);
    totalVm.store(t_info.virtual_size);

    return true;
}

#else

void ProcessReader::openCpuLoad()
{
}

qreal ProcessReader::readCpuLoad()
{
    return 0.0;
}

bool ProcessReader::readMemory()
{
    return false;
}

#endif
