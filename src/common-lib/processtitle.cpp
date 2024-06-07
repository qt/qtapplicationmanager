/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "processtitle.h"

#if !defined(Q_OS_LINUX)
QT_BEGIN_NAMESPACE_AM
void ProcessTitle::setTitle(const char *, ...) { }
void adjustArgumentCount(int &) { }
void ProcessTitle::augmentCommand(const char *) { }
const char *ProcessTitle::title() { return nullptr; }
QT_END_NAMESPACE_AM
#else

#include "logging.h"

#include <QVarLengthArray>
#include <QByteArray>

#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>


/* \internal

   How this works:
   All argv[] and envp[] strings are in one continuous memory region that starts at argv[0]. All
   strings are simply separated by '\0'. Sadly, Linux has no setproctitle() call like the BSDs
   have. You could just overwrite argv[0], but writing beyond the original length would overwrite
   the environment as seen by a process.

   In order to make room to extend argv[], we provide two solutions:
   1. Copy the environment somewhere else and use the "old" environment as a buffer to extend
      argv[]. This is the generic approach taken by the setTitle() function. This function mimics
      the API of the setproctitle() function found in BSDs:
      https://www.freebsd.org/cgi/man.cgi?query=setproctitle&sektion=3
      Unfortunately, the kernel keeps it's view to the old environment, which makes
      /proc/<pid>/environ hold part of the command line arguments instead of the actual
      environment.
   2. Use space already reserved with a placeholder argument at the end. This is the approach taken
      by the augmentCommand() function and solves the problem that /proc/<pid>/environ is broken by
      the first approach. For this to work we need to be able to insert the placeholder argument at
      process execution (and hence it doesn't work for processes started on the command line by the
      user).

   Both approaches need the ProcessTitleInitialize() function to run at process start-up. This is
   achieved by registering it as an .init function (which is a bit intrusive in a library like
   this, but kept for backwards compatibility).

*/

QT_BEGIN_NAMESPACE_AM

const char *ProcessTitle::placeholderArgument = "#placeholder-for-still-unknown-qtapplicationmanager-applicationid";

static char *startOfArgv = nullptr;    // original buffer (that ps uses), eventually with changed content
static size_t maxArgvSize = 0;
static char *originalArgv = nullptr;   // "original" in terms of content, but moved to differnt buffer
static size_t originalArgvSize = 0;
static int placeholders = 0;

static void ProcessTitleInitialize(int argc, char *argv[], char *envp[])
{
    // char *start = argv[0];
    // for (int i = 0; i < argc; ++i)
    //     fprintf(stderr, "ARGV[%d] = %p | len=%d | delta=%ld\n", i, argv[i], int(strlen(argv[i])), argv[i] - start);
    // for (int i = 0; envp[i]; ++i)
    //     fprintf(stderr, "ENVP[%d] = %p | len=%d | delta=%ld\n", i, envp[i], int(strlen(envp[i])), envp[i] - start);

    // sanity checks
    if (argc <= 0 || !argv[0] || !envp)
        return;

    char *lastArg = argv[argc - 1];
    const size_t lastArgSize = strlen(lastArg);
    if (envp[0] && ((lastArg + lastArgSize + 1) != envp[0]))
        return;

    // calculate the size of the available area
    startOfArgv = argv[0];
    originalArgvSize = size_t(envp[0] - argv[0]);
    originalArgv = static_cast<char *>(malloc(originalArgvSize));
    memcpy(originalArgv, argv[0], originalArgvSize);

    if (!strcmp(lastArg, ProcessTitle::placeholderArgument)) {
        placeholders = 1;
        // make sure ps doesn't print dummy placeholder argument
        memset(lastArg, 0, lastArgSize);
        // and /proc/<pid>/cmdline splits arguments with spaces
        for (int i = argc - 2; i > 0; --i)
            argv[i][-1] = ' ';
    } else {
        char *envpEnd;
        size_t envc = 0;

        while (envp[envc])
            ++envc;
        envpEnd = envp[envc - 1] + strlen(envp[envc - 1]) + 1;
        maxArgvSize = size_t(envpEnd - startOfArgv);

        // temporary copy of the list of pointers on the stack
        QVarLengthArray<char *, 2048> oldenvp(static_cast<int>(envc));
        memcpy(oldenvp.data(), envp, envc * sizeof(char *));

        // this will only free the list of pointers, but not the contents!
        clearenv();

        // copy the environment via setenv() - do NOT use putenv() as this would just put the old
        // pointer in the new table.
        for (int i = 0; i < oldenvp.size(); ++i) {
            // split into key/value pairs for setenv()
            char *name = oldenvp[i];
            char *value = strchr(name, '=');
            if (!value) // entries without '=' should not exist
                continue;
            *value++ = 0;
            if (setenv(name, value, 1) != 0) {
                fprintf(stderr, "ERROR: could not copy the environment: %s\n", strerror(errno));
                _exit(1);
            }
        }

        // fprintf(stderr, "env is moved: testing $SHELL=%s\n", getenv("SHELL"));
    }

    // we need to replace the argv[i] pointers with a copy of the original strings,
    // since the app's cmdline parser might want to access argv[i] later.
    for (int i = argc - 1; i >= 0; --i)
        argv[i] = originalArgv + (argv[i] - argv[0]);
}

// register as a .init function that is automatically run before main()
decltype(ProcessTitleInitialize) *init_ProcessTitleInitialize
    __attribute__((section(".init_array"), used)) = ProcessTitleInitialize;

void ProcessTitle::setTitle(const char *fmt, ...)
{
    if (!startOfArgv || maxArgvSize <= 0 || !originalArgv || originalArgvSize <= 0) {
        qWarning(LogSystem) << "ProcessTitle::setTitle() failed, because its initialization function"
                               " was not called via an .init_array section at process startup.";
        return;
    }

    char title[256];
    char *ptr = title;
    size_t len = 0;

    if (!fmt) {
        // reset to original argv[]
        ptr = originalArgv;
        len = originalArgvSize;
    } else {
        // BSD compatibility: the title will always start with the original argv[0] + ": ",
        // unless the first character in the format string is a '-'
        if (fmt[0] == '-') {
            fmt++;
        } else {
            len = qMin(strlen(originalArgv), sizeof(title) - 3);
            memcpy(title, originalArgv, len);
            memcpy(title + len, ": \0", 3);
            len += 2;
        }

        va_list ap;
        va_start(ap, fmt);
        size_t result = static_cast<size_t>(qvsnprintf(title + len, sizeof(title) - len, fmt, ap));
        va_end(ap);
        len += qMin(result, sizeof(title) - len); // clamp to buffer size in case of overflow
        if ((len + 1) > maxArgvSize)
            len = maxArgvSize - 1;
    }
    if (ptr && len) {
        memcpy(startOfArgv, ptr, len);
        memset(startOfArgv + len, 0, maxArgvSize - len);
    }
}

void ProcessTitle::adjustArgumentCount(int &argc)
{
    argc -= placeholders;
}

void ProcessTitle::augmentCommand(const char *extension)
{
    if (!startOfArgv || originalArgvSize <= 0 || placeholders == 0) {
        qWarning(LogSystem) << "ProcessTitle::augmentCommand() failed";
        return;
    }

    static const char *prefix = ": ";
    const size_t prefixLen = strlen(prefix);
    const size_t placeholderLen = strlen(placeholderArgument);
    const size_t commandLen = strlen(originalArgv);
    size_t augmentLen = strlen(extension) + prefixLen;
    if (augmentLen > placeholderLen)
        augmentLen = placeholderLen + 1;

    char *pos = startOfArgv + commandLen;
    // move real arguments to the right to make space for the command extension
    memmove(pos + augmentLen, pos, originalArgvSize - placeholderLen - commandLen - 2);
    // copy prefix and extension to this space
    memcpy(pos, prefix, prefixLen);
    strncpy(pos + prefixLen, extension, augmentLen - prefixLen);
}

const char *ProcessTitle::title()
{
    return startOfArgv;
}

QT_END_NAMESPACE_AM

#endif
