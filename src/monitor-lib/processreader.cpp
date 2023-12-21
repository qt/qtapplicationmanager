// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QMutexLocker>
#include "processreader.h"

#include "logging.h"

#if defined(Q_OS_MACOS)
#  include <mach/mach.h>
#elif defined(Q_OS_LINUX)
#  include <unistd.h>
#endif

QT_USE_NAMESPACE_AM

void ProcessReader::setProcessId(qint64 pid)
{
    m_pid = pid;
    if (pid)
        openCpuLoad();
}

void ProcessReader::enableMemoryReporting(bool enabled)
{
    m_memoryReportingEnabled = enabled;
    if (!m_memoryReportingEnabled)
        memory = Memory();
}

void ProcessReader::update()
{
    qreal load = readCpuLoad();

    if (m_memoryReportingEnabled) {
        Memory mem;
        bool memRead = readMemory(mem);
        QMutexLocker locker(&mutex);
        memory = memRead ? mem : Memory();
        cpuLoad = load;
    } else {
        QMutexLocker locker(&mutex);
        cpuLoad = load;
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
    m_lastCpuUsage = 0;
    m_elapsedTime.invalidate();
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

    if (!m_statReader || !m_statReader->isOpen()) {
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

    qreal load = elapsed ? (qreal(utime + stime - m_lastCpuUsage) * 1000 / qreal(sysconf(_SC_CLK_TCK)) / qreal(elapsed))
                         : 0.0;
    m_lastCpuUsage = utime + stime;
    return load;
}


bool ProcessReader::readMemory(Memory &mem)
{
    const QByteArray smapsFile = "/proc/" + QByteArray::number(m_pid) + "/smaps";
    return readSmaps(smapsFile, mem);
}

static uint parseValue(const char *pl) {
    while (*pl && (*pl < '0' || *pl > '9'))
        pl++;
    return static_cast<uint>(strtoul(pl, nullptr, 10));
}

bool ProcessReader::readSmaps(const QByteArray &smapsFile, Memory &mem)
{
    FILE *sf = nullptr;
    auto closeFile = qScopeGuard([=]() { if (sf) fclose(sf); });

    sf = fopen(smapsFile.constData(), "r");
    if (!sf)
        return false;

    const int lineLen = 100;  // we are not interested in full library paths
    char line[lineLen + 5];   // padding for highly unlikely trailing perm flags below
    char *pl;                 // pointer to chars within line
    bool ok = true;

    if (!fgets(line, lineLen, sf))
        return false;

    // sanity checks
    for (pl = line; pl < (line + 4) && ok; ++pl)
        ok = ((*pl >= '0' && *pl <= '9') || (*pl >= 'a' && *pl <= 'f'));
    while (strlen(line) == lineLen - 1 && line[lineLen - 2] != '\n') {
        if (Q_UNLIKELY(!fgets(line, lineLen, sf)))
            break;
    }
    if (!fgets(line, lineLen, sf))
        return false;
    static const char strSize[] = "Size: ";
    ok = ok && !qstrncmp(line, strSize, sizeof(strSize) - 1);
    if (!ok)
        return false;

    // Determine block size
    ok = false;
    int blockLen = 0;
    while (fgets(line, lineLen, sf) && !ok) {
        if (!(line[0] < '0' || line[0] > '9') && (line[0] < 'a' || line[0] > 'f'))
            ok = true;
        ++blockLen;
    }
    if (!ok || blockLen < 12 || blockLen > 32)
        return false;

    fseek(sf, 0, SEEK_SET);
    bool wasPrivateOnly = false;
    ok = false;

    while (true) {
        if (Q_UNLIKELY(!fgets(line, lineLen, sf))) {
            ok = feof(sf);
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
            if (Q_UNLIKELY(!fgets(line, lineLen, sf)))
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
            if (Q_UNLIKELY(!fgets(line, lineLen, sf)))
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

        mem.totalVm += vm;
        mem.totalRss += rss;
        mem.totalPss += pss;

        static const char permRXP[] = { 'r', '-', 'x', 'p' };
        static const char permRWP[] = { 'r', 'w', '-', 'p' };
        if (!memcmp(permissions, permRXP, sizeof(permissions))) {
            mem.textVm += vm;
            mem.textRss += rss;
            mem.textPss += pss;
        } else if (!memcmp(permissions, permRWP, sizeof(permissions))
                   && !isMainStack && (vm != 8192 || hasInode || !wasPrivateOnly) // try to exclude stack
                   && !hasInode) {
            mem.heapVm += vm;
            mem.heapRss += rss;
            mem.heapPss += pss;
        }

        static const char permP[] = { '-', '-', '-', 'p' };
        wasPrivateOnly = !memcmp(permissions, permP, sizeof(permissions));

        for (int skip = skipLen; skip; --skip) {
            if (Q_UNLIKELY(!fgets(line, lineLen, sf)))
                break;
        }
    }

    return ok;
}

bool ProcessReader::testReadSmaps(const QByteArray &smapsFile)
{
    memory = Memory();
    return readSmaps(smapsFile, memory);
}

#elif defined(Q_OS_MACOS)

void ProcessReader::openCpuLoad()
{
}

qreal ProcessReader::readCpuLoad()
{
    return 0.0;
}

bool ProcessReader::readMemory(Memory &mem)
{
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO,
                                  reinterpret_cast<task_info_t>(&t_info), &t_info_count)) {
        qCWarning(LogSystem) << "Could not read memory data";
        return false;
    }

    mem.totalRss = t_info.resident_size;
    mem.totalVm = t_info.virtual_size;

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

bool ProcessReader::readMemory(Memory &mem)
{
    Q_UNUSED(mem)
    return false;
}

#endif

#include "moc_processreader.cpp"
