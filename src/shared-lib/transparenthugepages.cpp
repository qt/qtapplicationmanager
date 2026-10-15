// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "transparenthugepages.h"

#if defined(Q_OS_LINUX)
#  include "logging.h"

#  include <sys/prctl.h>
#  include <stdlib.h>
#  include <string.h>
#  include <unistd.h>
#  include <errno.h>

#  ifndef PR_THP_DISABLE_EXCEPT_ADVISED
#    define PR_THP_DISABLE_EXCEPT_ADVISED (1 << 1)
#  endif
#endif

QT_BEGIN_NAMESPACE_AM

namespace TransparentHugePages {

#if defined(Q_OS_LINUX)

enum class State {
    NotSupported,
    NotRequested,
    SystemDefault,
    Disabled,
    DisabledUnlessAdvised,
};

struct THPGlobal
{
    State requestedState = State::NotRequested;
    int prctlError = 0;
    bool unknownEnvVarValue = false;
};

static THPGlobal &thpg()
{
    static THPGlobal instance;
    return instance;
}

// this will make it run before all other static constructor functions
static void init() __attribute__((constructor(101)));

static void init()
{
    // Make sure to NEVER allocate from this function!

    const char *thp = ::getenv("AM_TRANSPARENT_HUGE_PAGES");

    if (thp && *thp) {
        int disable = -1;
        int flag = 0;

        if (::strcmp(thp, "disable") == 0) {
            disable = 1;
            thpg().requestedState = State::Disabled;
        } else if (::strcmp(thp, "madvise") == 0) {
            disable = 1;
            flag = PR_THP_DISABLE_EXCEPT_ADVISED;
            thpg().requestedState = State::DisabledUnlessAdvised;
        } else if (::strcmp(thp, "enable") == 0) {
            disable = 0;
            thpg().requestedState = State::SystemDefault;
        } else {
            thpg().unknownEnvVarValue = true;
        }

        if (disable != -1) {
            if (::prctl(PR_SET_THP_DISABLE, disable, flag, 0, 0) < 0)
                thpg().prctlError = errno;
        }
    }
}

void checkState()
{
    auto stateToString = [](State state) {
        switch (state) {
        case State::SystemDefault:         return "on system default";
        case State::Disabled:              return "disabled";
        case State::DisabledUnlessAdvised: return "disabled, unless advised";
        case State::NotSupported:          return "not supported";
        default:
        case State::NotRequested:          return ""; // never reached
        }
    };

    const State state = [] {
        switch (::prctl(PR_GET_THP_DISABLE, 0, 0, 0, 0)) {
        case 0b00: return State::SystemDefault;
        case 0b01: return State::Disabled;
        case 0b11: return State::DisabledUnlessAdvised;
        default:
        case -1:   return State::NotSupported;
        }
    }();

    if (thpg().unknownEnvVarValue) {
        qCWarning(LogSystem) << "Unknown value for AM_TRANSPARENT_HUGE_PAGES:"
                             << qEnvironmentVariable("AM_TRANSPARENT_HUGE_PAGES")
                             << "(expected: 'disable', 'madvise', or 'enable')";
    } else if ((thpg().requestedState != State::NotRequested) && (thpg().requestedState != state)) {
        if (thpg().prctlError) {
            qCWarning(LogSystem) << "Transparent huge pages are" << stateToString(state) << "although"
                                 << stateToString(thpg().requestedState) << "was requested:"
                                 << ::strerror(thpg().prctlError);
        } else {
            qCWarning(LogSystem) << "Transparent huge pages are" << stateToString(state) << "although"
                                 << stateToString(thpg().requestedState) << "was requested";
        }
    } else if (thpg().requestedState != State::NotRequested) {
        qCInfo(LogSystem) << "Transparent huge pages are" << stateToString(state);
    } else {
        qCDebug(LogSystem) << "Transparent huge pages are" << stateToString(state);
    }
}

#else

void checkState() { }

#endif // Q_OS_LINUX

} // namespace TransparentHugePages

QT_END_NAMESPACE_AM
