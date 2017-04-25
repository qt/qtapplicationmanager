/****************************************************************************
**
** Copyright (C) 2017 Pelagicore AG
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

#include "processtitle.h"

#if !defined(Q_OS_LINUX)
QT_BEGIN_NAMESPACE_AM
void ProcessTitle::setTitle(const char *, ...) { }
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

QT_BEGIN_NAMESPACE_AM

static char *startOfArgv = nullptr;
static size_t maxArgvSize = 0;
static char *originalArgv = nullptr;
static size_t originalArgvSize = 0;

/* \internal
    Sadly, Linux has no setproctitle() call like the BSDs have...
    You could just overwrite argv[0], but writing beyond the original length would overwrite the
    environment.
*/
static void ProcessTitleInitialize(int argc, char *argv[], char *envp[])
{
    // How this works:
    // All argv[] and envp[] strings are in one continuous memory region that starts at argv[0]. All
    // strings are simply separated by '\0'.
    // In order to make room to extend argv[], we copy the environment somewhere else and can then
    // use the "old" environment as a buffer to extend argv[]
    // In addition, we need to replace the argv[i] pointers with a copy of the original strings,
    // since the app's cmdline parser might want to access argv[i] after a setTitle call.

    //    char *start = argv[0];
    //    for (int i = 0; i < argc; ++i)
    //        fprintf(stderr, "ARGV[%d] = %p | len=%d | delta=%ld\n", i, argv[i], int(strlen(argv[i])), argv[i] - start);
    //    for (int i = 0; envp[i]; ++i)
    //        fprintf(stderr, "ENVP[%d] = %p | len=%d | delta=%ld\n", i, envp[i], int(strlen(envp[i])), envp[i] - start);

    // sanity checks
    if (argc <= 0 || !argv[0] || !envp)
        return;
    if (envp[0] && ((argv[argc - 1] + strlen(argv[argc - 1]) + 1) != envp[0]))
        return;

    // calculate the size of the available area
    startOfArgv = argv[0];
    originalArgvSize = envp[0] - argv[0];
    originalArgv = (char *) malloc(originalArgvSize);
    memcpy(originalArgv, argv[0], originalArgvSize);
    char *envpEnd;
    int envc = 0;

    while (envp[envc])
        ++envc;
    envpEnd = envp[envc - 1] + strlen(envp[envc - 1]) + 1;
    maxArgvSize = envpEnd - startOfArgv;

    // temporary copy of the list of pointers on the stack
    QVarLengthArray<char *, 2048> oldenvp(envc);
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

    // last step: make sure that argv[i] returns consistent pointers regardless of how many times
    // setTitle() will be called
    for (int i = argc - 1; i >= 0; --i)
        argv[i] = originalArgv + (argv[i] - argv[0]);

    // fprintf(stderr, "env is moved: testing $SHELL=%s\n", getenv("SHELL"));
}

// register as a a .init function that is automatically run before main()
decltype(ProcessTitleInitialize) *init_ProcessTitleInitialize
    __attribute__((section(".init_array"))) = ProcessTitleInitialize;


/*! \internal
    This function mimics the API of the setproctitle() function found in BSDs:
    https://www.freebsd.org/cgi/man.cgi?query=setproctitle&sektion=3
*/
void ProcessTitle::setTitle(const char *fmt, ...)
{
    if (!startOfArgv || maxArgvSize <= 0 || !originalArgv || originalArgvSize <= 0) {
        qCritical(LogSystem) << "ERROR: called ProcessTitle::setTitle(), but the initialization function was not called via .init_array beforehand.";
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
        size_t result = qvsnprintf(title + len, sizeof(title) - len, fmt, ap);
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

const char *ProcessTitle::title()
{
    if (!startOfArgv || maxArgvSize <= 0 || !originalArgv || originalArgvSize <= 0) {
        qCritical(LogSystem) << "ERROR: called ProcessTitle::title(), but the initialization function was not called via .init_array beforehand.";
        return nullptr;
    }
    return startOfArgv;
}

QT_END_NAMESPACE_AM

#endif
