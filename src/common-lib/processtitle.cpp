// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "processtitle.h"

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE // for program_invocation_short_name
#  endif
#  include <cerrno>
#  include <unistd.h>
#  include <sys/syscall.h>
#  include <sys/prctl.h>
#  include <zlib.h>

#  include <QFile>
#  include <QSysInfo>
#  include <QVersionNumber>
#  include "exception.h"
#  include "logging.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

// This function is not necessary, but it helps the user diagnose a failing setTitle() call
// 1: yes or module | 0: no | -1: unknown
static int hasKernelConfig(const char *config)
{
    const QString kernel = QSysInfo::kernelVersion();
    const QByteArray configKey = "\n"_ba + config + '=';

    for (QString path : { u"/proc/config"_s, u"/proc/config.gz"_s,
                         u"/boot/config-${k}"_s, u"/lib/modules/${k}/build/.config"_s }) {
        path.replace(u"${k}"_s, kernel);
        QByteArray content;
        if (auto f = ::gzopen(qPrintable(path), "r")) {
            std::array<char, 4096> buffer;
            while (true) {
                if (int result = ::gzread(f, buffer.data(), buffer.size()); result > 0)
                    content.append(buffer.data(), result);
                else
                    break;
            }
            ::gzclose(f);

            const auto pos = content.indexOf(configKey);
            if ((pos >= 0) && ((pos + configKey.size()) < content.size())) {
                const char configValue = content.at(pos + configKey.size());
                return ((configValue == 'y') || (configValue == 'm')) ? 1 : 0;
            } else {
                return 0;
            }
        }
    }
    return -1;
}

Q_GLOBAL_STATIC(QByteArray, originalProgramInvocationShortName, program_invocation_short_name);
static const char *currentTitle = nullptr; // this cannot be deleted at shutdown!

void ProcessTitle::setTitle(QByteArrayView title)
{
    try {
        // man setproctitle (BSD only)
        const QByteArray t = [&]() -> QByteArray {
            if (title.isEmpty())
                return *originalProgramInvocationShortName;
            else if (title.startsWith('-'))
                return title.toByteArray();
            else
                return *originalProgramInvocationShortName + ": " + title;
        }();

        QFile procSelfStat(u"/proc/self/stat"_s);
        if (!procSelfStat.open(QIODevice::ReadOnly))
            throw Exception(procSelfStat, "failed to open");

        const auto statBuffer = procSelfStat.readAll();
        if (statBuffer.isEmpty())
            throw Exception(procSelfStat, "failed to read");

        // The second field is the process name in parentheses. It may contain spaces, so we
        // need to find the last ')' to start our actual parsing
        auto endOfNamePos = statBuffer.lastIndexOf(')');
        if (endOfNamePos < 1)
            throw Exception("could not parse %1").arg(procSelfStat.fileName());

        // Skip ") ", then split the rest of the line by spaces
        const auto statList = statBuffer.sliced(endOfNamePos + 2).split(' ');
        if (statList.size() < 50)
            throw Exception("not enough fields in %1").arg(procSelfStat.fileName());

        // man 5 proc_pid_stat
        // Since we started parsing after the process name, we have a -2 offset.
        // We also add another -1, because the kernel docs are 1-based.
        auto parseField = [&](int index) -> __u64 {
            bool ok = false;
            if (auto f = statList.at(index - 2 - 1).toULongLong(&ok); ok)
                return f;
            else
                throw Exception("could not parse field %1 in %2").arg(index).arg(procSelfStat.fileName());
        };

        auto argStart = std::make_unique<char[]>(t.size() + 1);
        qstrcpy(argStart.get(), t.constData());

        struct ::prctl_mm_map map {
            .start_code  = parseField(26),
            .end_code    = parseField(27),
            .start_data  = parseField(45),
            .end_data    = parseField(46),
            .start_brk   = parseField(47),
            .brk         = __u64(::syscall(__NR_brk, 0)),
            .start_stack = parseField(28),
            .arg_start   = __u64(argStart.get()),
            .arg_end     = __u64(argStart.get() + t.size() + 1),
            .env_start   = parseField(50),
            .env_end     = parseField(51),
            .auxv        = 0ULL,
            .auxv_size   = 0U,
            .exe_fd      = -1U,
        };

        if (::prctl(PR_SET_MM, __u64(PR_SET_MM_MAP), &map, __u64(sizeof(map)), 0ULL) != 0) {
            if (errno == EINVAL) {
                if (QVersionNumber::fromString(QSysInfo::kernelVersion()) < QVersionNumber(3, 18))
                    throw Exception("the kernel is older than 3.18");
            } else if (errno == EPERM) {
                if (hasKernelConfig("CONFIG_CHECKPOINT_RESTORE") == 0)
                    throw Exception("the kernel is missing CONFIG_CHECKPOINT_RESTORE");
            }
            throw Exception(errno, "prctl(PR_SET_MM_MAP)");
        }

        program_invocation_short_name = argStart.get();

        delete [] currentTitle;
        currentTitle = argStart.release();
    } catch (const Exception &e) {
        qCWarning(LogSystem).noquote() << "ProcessTitle::setTitle() failed:" << e.errorString();
    }
}

const char *ProcessTitle::title()
{
    return currentTitle ? currentTitle : originalProgramInvocationShortName->constData();
}

QT_END_NAMESPACE_AM

#else // defined Q_OS_LINUX && !defined(Q_OS_ANDROID)
QT_BEGIN_NAMESPACE_AM
void ProcessTitle::setTitle(QByteArrayView) { }
const char *ProcessTitle::title() { return nullptr; }
QT_END_NAMESPACE_AM
#endif
