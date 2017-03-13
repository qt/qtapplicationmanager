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

#ifdef _WIN32
// needed for QueryFullProcessImageNameW
#  define WINVER _WIN32_WINNT_VISTA
#  define _WIN32_WINNT _WIN32_WINNT_VISTA
#endif

#include <QFileInfo>
#include <QFile>
#include <QCoreApplication>
#include <QNetworkInterface>

#include "utilities.h"
#include "exception.h"
#include "unixsignalhandler.h"

#include <errno.h>

#if defined(Q_OS_UNIX)
#  include <unistd.h>
#  include <sys/ioctl.h>
#  include <termios.h>
#  include <signal.h>
#endif
#if defined(Q_OS_WIN)
#  include <QThread>
#  include <windows.h>
#  include <io.h>
#  include <tlhelp32.h>
#elif defined(Q_OS_OSX)
#  include <unistd.h>
#  include <sys/mount.h>
#  include <sys/statvfs.h>
#  include <sys/sysctl.h>
#  include <libproc.h>
#else
#  include <stdio.h>
#  include <mntent.h>
#  include <sys/stat.h>
#  if defined(Q_OS_ANDROID)
#    include <sys/vfs.h>
#    define statvfs statfs
#  else
#    include <sys/statvfs.h>
#  endif
#  include <qplatformdefs.h>

// the process environment from the C library
extern char **environ;
#endif

QT_BEGIN_NAMESPACE_AM

QString hardwareId()
{
#if defined(AM_HARDWARE_ID)
    return QString::fromLocal8Bit(AM_HARDWARE_ID);
#elif defined(AM_HARDWARE_ID_FROM_FILE)
    QFile f(QString::fromLocal8Bit(AM_HARDWARE_ID_FROM_FILE));
    if (f.open(QFile::ReadOnly))
        return f.readAll().trimmed();
#else
    foreach (const QNetworkInterface &iface, QNetworkInterface::allInterfaces()) {
        if (iface.isValid() && (iface.flags() & QNetworkInterface::IsUp)
                && !(iface.flags() & (QNetworkInterface::IsPointToPoint | QNetworkInterface::IsLoopBack))
                && !iface.hardwareAddress().isEmpty()) {
            return iface.hardwareAddress().replace(qL1C(':'), qL1S("-"));
        }
    }
#endif
    return QString();
}

bool ensureCorrectLocale()
{
    // We need to make sure we are running in a Unicode locale, since we are
    // running into problems when unpacking packages with libarchive that
    // contain non-ASCII characters.
    // This has to be done *before* the QApplication constructor to avoid
    // the initialization of the text codecs.

#if defined(Q_OS_WIN) || defined(Q_OS_OSX)
    // Windows is UTF16
    // Mac is UTF8
    return true;
#else

    // check if umlaut-a converts correctly
    auto checkUtf = []() -> bool {
        const wchar_t umlaut_w[2] = { 0x00e4, 0x0000 };
        char umlaut_mb[2];
        int umlaut_mb_len = wcstombs(umlaut_mb, umlaut_w, sizeof(umlaut_mb));

        return (umlaut_mb_len == 2 && umlaut_mb[0] == '\xc3' && umlaut_mb[1] == '\xa4');
    };

    // init locales from env variables
    setlocale(LC_ALL, "");

    // check if this is enough to have an UTF-8 locale
    if (checkUtf())
        return true;

    // LC_ALL trumps all the rest, so if this is not specifying an UTF-8 locale, we need to unset it
    QByteArray lc_all = qgetenv("LC_ALL");
    if (!lc_all.isEmpty() && !(lc_all.endsWith(".UTF-8") || lc_all.endsWith(".UTF_8"))) {
        unsetenv("LC_ALL");
        setlocale(LC_ALL, "");
    }

    // check again
    if (checkUtf())
        return true;

    // now the time-consuming part: trying to switch to a well-known UTF-8 locale
    const char *locales[] = { "C.UTF-8", "en_US.UTF-8", nullptr };
    for (const char **loc = locales; *loc; ++loc) {
        if (const char *old = setlocale(LC_CTYPE, *loc)) {
            if (checkUtf()) {
                if (setenv("LC_CTYPE", *loc, 1) == 0)
                    return true;
            }
            setlocale(LC_CTYPE, old);
        }
    }

    return false;
#endif
}

bool checkCorrectLocale()
{
    // see ensureCorrectLocale() above. Call this after the QApplication
    // constructor as a sanity check.
#if defined(Q_OS_WIN) || defined(Q_OS_OSX)
    return true;
#else
    // check if umlaut-a converts correctly
    return QString::fromUtf8("\xc3\xa4").toLocal8Bit() == "\xc3\xa4";
#endif
}

/*! \internal
    Check a YAML document against the "standard" AM header.
    If \a numberOfDocuments is positive, the number of docs need to match exactly. If it is
    negative, the \a numberOfDocuments is taken as the required minimum amount of documents.

*/
void checkYamlFormat(const QVector<QVariant> &docs, int numberOfDocuments,
                     const QVector<QByteArray> &formatTypes, int formatVersion) Q_DECL_NOEXCEPT_EXPR(false)
{
    int actualSize = docs.size();
    QByteArray actualFormatType;
    int actualFormatVersion = 0;

    if (actualSize >= 1) {
        const auto map = docs.constFirst().toMap();
        actualFormatType = map.value(qSL("formatType")).toString().toUtf8();
        actualFormatVersion = map.value(qSL("formatVersion")).toInt(0);
    }

    if (numberOfDocuments < 0) {
        if (actualSize < numberOfDocuments) {
            throw Exception("wrong number of YAML documents: expected at least %1, got %2")
                .arg(-numberOfDocuments).arg(actualSize);
        }
    } else {
        if (actualSize != numberOfDocuments) {
            throw Exception("wrong number of YAML documents: expected %1, got %2")
                .arg(numberOfDocuments).arg(actualSize);
        }
    }
    if (!formatTypes.contains(actualFormatType)) {
        throw Exception("wrong formatType header: expected %1, got %2")
            .arg(QString::fromUtf8(formatTypes.toList().join(", or ")), QString::fromUtf8(actualFormatType));
    }
    if (actualFormatVersion != formatVersion) {
        throw Exception("wrong formatVersion header: expected %1, got %2")
                .arg(formatVersion).arg(actualFormatVersion);
    }
}

bool diskUsage(const QString &path, quint64 *bytesTotal, quint64 *bytesFree)
{
    QString cpath = QFileInfo(path).canonicalPath();

#if defined(Q_OS_WIN)
    return GetDiskFreeSpaceExW((LPCWSTR) cpath.utf16(), (ULARGE_INTEGER *) bytesFree,
                               (ULARGE_INTEGER *) bytesTotal, nullptr);

#else // Q_OS_UNIX
    int result;
    struct ::statvfs svfs;

    do {
        result = ::statvfs(cpath.toLocal8Bit(), &svfs);
        if (result == -1 && errno == EINTR)
            continue;
    } while (false);

    if (result == 0) {
        if (bytesTotal)
            *bytesTotal = quint64(svfs.f_frsize) * svfs.f_blocks;
        if (bytesFree)
            *bytesFree = quint64(svfs.f_frsize) * svfs.f_bavail;
        return true;
    }
    return false;
#endif // Q_OS_WIN
}

QMultiMap<QString, QString> mountedDirectories()
{
    QMultiMap<QString, QString> result;
#if defined(Q_OS_WIN)
    return result; // no mounts on Windows

#elif defined(Q_OS_OSX)
    struct statfs *sfs = nullptr;
    int count = getmntinfo(&sfs, MNT_NOWAIT);

    for (int i = 0; i < count; ++i, ++sfs) {
        result.insertMulti(QString::fromLocal8Bit(sfs->f_mntonname), QString::fromLocal8Bit(sfs->f_mntfromname));
    }
#else
    QFile fpm(qSL("/proc/self/mounts"));
    if (!fpm.open(QFile::ReadOnly))
        return result;
    QByteArray mountBuffer = fpm.readAll();

    FILE *pm = fmemopen(mountBuffer.data(), mountBuffer.size(), "r");
    if (!pm)
        return result;

#  if defined(Q_OS_ANDROID)
    while (struct mntent *mntPtr = getmntent(pm)) {
        result.insertMulti(QString::fromLocal8Bit(mntPtr->mnt_dir),
                           QString::fromLocal8Bit(mntPtr->mnt_fsname));
    }
#  else
    int pathMax = pathconf("/", _PC_PATH_MAX) * 2 + 1024;  // quite big, but better be safe than sorry
    QScopedArrayPointer<char> strBuf(new char[pathMax]);
    struct mntent mntBuf;

    while (getmntent_r(pm, &mntBuf, strBuf.data(), pathMax - 1)) {
        result.insertMulti(QString::fromLocal8Bit(mntBuf.mnt_dir),
                           QString::fromLocal8Bit(mntBuf.mnt_fsname));
    }
#  endif
    fclose(pm);
#endif

    return result;
}


bool isValidDnsName(const QString &dnsName, bool isAliasName, QString *errorString)
{
    static const int maxLength = 150;

    try {
        // AM specific checks first
        // we need to make sure that we can use the name as directory on a FAT formatted SD-card,
        // which has a 255 character path length restriction

        if (dnsName.length() > maxLength)
            throw Exception(Error::Parse, "the maximum length is %1 characters (found %2 characters)").arg(maxLength, dnsName.length());

        // we require at least 3 parts: tld.company.app
        QStringList parts = dnsName.split('.');
        if (parts.size() < 3)
            throw Exception(Error::Parse, "the minimum amount of parts (subdomains) is 3 (found %1)").arg(parts.size());

        // standard RFC compliance tests (RFC 1035/1123)

        auto partCheck = [](const QString &part, const char *type) {
            int len = part.length();

            if (len < 1 || len > 63)
                throw Exception(Error::Parse, "%1 must consist of at least 1 and at most 63 characters (found %2 characters)").arg(qL1S(type)).arg(len);

            for (int pos = 0; pos < len; ++pos) {
                ushort ch = part.at(pos).unicode();
                bool isFirst = (pos == 0);
                bool isLast  = (pos == (len - 1));
                bool isDash  = (ch == '-');
                bool isDigit = (ch >= '0' && ch <= '9');
                bool isLower = (ch >= 'a' && ch <= 'z');

                if ((isFirst || isLast || !isDash) && !isDigit && !isLower)
                    throw Exception(Error::Parse, "%1 must consists of only the characters '0-9', 'a-z', and '-' (which cannot be the first or last character)").arg(qL1S(type));
            }
        };

        if (isAliasName) {
            QString aliasTag = parts.last();
            int sepPos = aliasTag.indexOf(qL1C('@'));
            if (sepPos < 0 || sepPos == (aliasTag.size() - 1))
                throw Exception(Error::Parse, "missing alias-id tag");

            parts.removeLast();
            parts << aliasTag.left(sepPos);
            parts << aliasTag.mid(sepPos + 1);
        }

        for (int i = 0; i < parts.size(); ++i) {
            const QString &part = parts.at(i);
            partCheck(part, isAliasName && (i == (part.size() - 1)) ? "tag name" : "parts (subdomains)");
        }

        return true;
    } catch (const Exception &e) {
        if (errorString)
            *errorString = e.errorString();
        return false;
    }
}

int versionCompare(const QString &version1, const QString &version2)
{
    int pos1 = 0;
    int pos2 = 0;
    int len1 = version1.length();
    int len2 = version2.length();

    forever {
        if (pos1 == len1 && pos2 == len2)
            return 0;
       else if (pos1 >= len1)
            return -1;
        else if (pos2 >= len2)
            return +1;

        QString part1 = version1.mid(pos1++, 1);
        QString part2 = version2.mid(pos2++, 1);

        bool isDigit1 = part1.at(0).isDigit();
        bool isDigit2 = part2.at(0).isDigit();

        if (!isDigit1 || !isDigit2) {
            int cmp = part1.compare(part2);
            if (cmp)
                return (cmp > 0) ? 1 : -1;
        } else {
            while ((pos1 < len1) && version1.at(pos1).isDigit())
                part1.append(version1.at(pos1++));
            while ((pos2 < len2) && version2.at(pos2).isDigit())
                part2.append(version2.at(pos2++));

            int num1 = part1.toInt();
            int num2 = part2.toInt();

            if (num1 != num2)
                return (num1 > num2) ? 1 : -1;
        }
    }
}


TemporaryDir::TemporaryDir()
    : QTemporaryDir()
{}

TemporaryDir::TemporaryDir(const QString &templateName)
    : QTemporaryDir(templateName)
{}

TemporaryDir::~TemporaryDir()
{
    recursiveOperation(path(), SafeRemove());
}

bool SafeRemove::operator()(const QString &path, RecursiveOperationType type)
{
   static const QFileDevice::Permissions fullAccess =
           QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
           QFileDevice::ReadUser  | QFileDevice::WriteUser  | QFileDevice::ExeUser  |
           QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup |
           QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;

   switch (type) {
   case RecursiveOperationType::EnterDirectory:
       return QFile::setPermissions(path, fullAccess);

   case RecursiveOperationType::LeaveDirectory: {
        // QDir cannot delete the directory it is pointing to
       QDir dir(path);
       QString dirName = dir.dirName();
       return dir.cdUp() && dir.rmdir(dirName);
   }
   case RecursiveOperationType::File:
       return QFile::remove(path);
   }
   return false;
}

#if defined(Q_OS_UNIX)

SetOwnerAndPermissions::SetOwnerAndPermissions(uid_t user, gid_t group, mode_t permissions)
    : m_user(user)
    , m_group(group)
    , m_permissions(permissions)
{ }


bool SetOwnerAndPermissions::operator()(const QString &path, RecursiveOperationType type)
{
    if (type == RecursiveOperationType::EnterDirectory)
        return true;

    const QByteArray localPath = path.toLocal8Bit();
    mode_t mode = m_permissions;

    if (type == RecursiveOperationType::LeaveDirectory) {
        // set the x bit for directories, but only where it makes sense
        if (mode & 06)
            mode |= 01;
        if (mode & 060)
            mode |= 010;
        if (mode & 0600)
            mode |= 0100;
    }

    return ((chmod(localPath, mode) == 0) && (chown(localPath, m_user, m_group) == 0));
}

#endif // Q_OS_UNIX


#if defined(Q_OS_ANDROID)

QT_END_NAMESPACE_AM
#include <QtAndroidExtras>
QT_BEGIN_NAMESPACE_AM

QString findOnSDCard(const QString &file)
{
    QAndroidJniObject activity = QtAndroid::androidActivity();
    QAndroidJniObject dirs = activity.callObjectMethod("getExternalFilesDirs", "(Ljava/lang/String;)[Ljava/io/File;", NULL);

    if (dirs.isValid()) {
        QAndroidJniEnvironment env;
        for (int i = 0; i < env->GetArrayLength(dirs.object<jarray>()); ++i) {
            QAndroidJniObject dir = env->GetObjectArrayElement(dirs.object<jobjectArray>(), i);

            QString path = QDir(dir.toString()).absoluteFilePath(file);

            if (QFile(path).open(QIODevice::ReadOnly)) {
                qWarning(" ##### found %s on external dir [%d] path: %s", qPrintable(file), i, qPrintable(dir.toString()));
                return path;
            }
        }
    }
    return QString();
}

#endif

void getOutputInformation(bool *ansiColorSupport, bool *runningInCreator, int *consoleWidth)
{
    static bool ansiColorSupportDetected = false;
    static bool runningInCreatorDetected = false;
    static QAtomicInteger<int> consoleWidthCached(0);
    static int consoleWidthCalculated = -1;
    static bool once = false;

    if (!once) {
        enum { ColorAuto, ColorOff, ColorOn } forceColor = ColorAuto;
        QByteArray forceColorOutput = qgetenv("AM_FORCE_COLOR_OUTPUT");
        if (forceColorOutput == "off" || forceColorOutput == "0")
            forceColor = ColorOff;
        else if (forceColorOutput == "on" || forceColorOutput == "1")
            forceColor = ColorOn;

#if defined(Q_OS_UNIX)
        if (::isatty(STDERR_FILENO))
            ansiColorSupportDetected = true;

#elif defined(Q_OS_WIN)
        HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
        if (h != INVALID_HANDLE_VALUE) {
            if (QSysInfo::windowsVersion() >= QSysInfo::WV_10_0) {
                // enable ANSI mode on Windows 10
                DWORD mode = 0;
                if (GetConsoleMode(h, &mode)) {
                    mode |= 0x04;
                    if (SetConsoleMode(h, mode))
                        ansiColorSupportDetected = true;
                }
            }
        }
#endif

        qint64 pid = QCoreApplication::applicationPid();
        forever {
            pid = getParentPid(pid);
            if (!pid)
                break;

#if defined(Q_OS_LINUX)
            static QString checkCreator = qSL("/proc/%1/exe");
            QFileInfo fi(checkCreator.arg(pid));
            if (fi.readLink().contains(qSL("qtcreator"))) {
                runningInCreatorDetected = true;
                break;
            }
#elif defined(Q_OS_OSX)
            static char buffer[PROC_PIDPATHINFO_MAXSIZE + 1];
            int len = proc_pidpath(pid, buffer, sizeof(buffer) - 1);
            if ((len > 0) && QByteArray::fromRawData(buffer, len).contains("Qt Creator")) {
                runningInCreatorDetected = true;
                break;
            }
#elif defined(Q_OS_WIN)
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid);
            if (hProcess) {
                wchar_t exeName[1024] = { 0 };
                DWORD exeNameSize = sizeof(exeName) - 1;
                if (QueryFullProcessImageNameW(hProcess, 0, exeName, &exeNameSize)) {
                    if (QString::fromWCharArray(exeName, exeNameSize).contains(qSL("qtcreator.exe")))
                        runningInCreatorDetected = true;
                }
            }
#endif
        }

        if (forceColor != ColorAuto)
            ansiColorSupportDetected = (forceColor == ColorOn);
        else if (!ansiColorSupportDetected)
            ansiColorSupportDetected = runningInCreatorDetected;

#if defined(Q_OS_UNIX) && defined(SIGWINCH)
        UnixSignalHandler::instance()->install(UnixSignalHandler::RawSignalHandler, SIGWINCH, [](int) {
            consoleWidthCached = 0;
        });
#elif defined(Q_OS_WIN)
        class ConsoleThread : public QThread
        {
        public:
            ConsoleThread(QObject *parent)
                : QThread(parent)
            { }
        protected:
            void run() override
            {
                HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
                DWORD mode = 0;
                if (!GetConsoleMode(h, &mode))
                    return;
                if (!SetConsoleMode(h, mode | ENABLE_WINDOW_INPUT))
                    return;

                INPUT_RECORD ir;
                DWORD irRead = 0;
                while (ReadConsoleInputW(h, &ir, 1, &irRead)) {
                    if ((irRead == 1) && (ir.EventType == WINDOW_BUFFER_SIZE_EVENT))
                        consoleWidthCached = 0;
                }
            }
        };
        (new ConsoleThread(qApp))->start();
#endif // Q_OS_WIN
        once = true;
    }

    if (ansiColorSupport)
        *ansiColorSupport = ansiColorSupportDetected;
    if (runningInCreator)
        *runningInCreator = runningInCreatorDetected;
    if (consoleWidth) {
        if (!consoleWidthCached) {
            consoleWidthCached = 1;
            consoleWidthCalculated = -1;
#if defined(Q_OS_UNIX)
            if (::isatty(STDERR_FILENO)) {
                struct ::winsize ws;
                if ((::ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == 0) && (ws.ws_col > 0))
                    consoleWidthCalculated = ws.ws_col;
            }
#elif defined(Q_OS_WIN)
            HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
            if (h != INVALID_HANDLE_VALUE && h != NULL) {
                CONSOLE_SCREEN_BUFFER_INFO csbi;
                if (GetConsoleScreenBufferInfo(h, &csbi))
                    consoleWidthCalculated = csbi.dwSize.X;
            }
#endif
        }
        if ((consoleWidthCalculated <= 0) && runningInCreatorDetected)
            *consoleWidth = 120;
        else
            *consoleWidth = consoleWidthCalculated;
    }
}

qint64 getParentPid(qint64 pid)
{
    qint64 ppid = 0;

#if defined(Q_OS_LINUX)
    static QString proc = qSL("/proc/%1/stat");
    QFile f(proc.arg(pid));
    if (f.open(QIODevice::ReadOnly)) {
        // we need just the 4th field, but the 2nd is the binary name, which could be long
        QByteArray ba = f.read(512);
        // the binary name could contain ')' and/or ' ' and the kernel escapes neither...
        int pos = ba.lastIndexOf(')');
        if (pos > 0 && ba.length() > (pos + 5))
            ppid = strtoll(ba.constData() + pos + 4, nullptr, 10);
    }

#elif defined(Q_OS_OSX)
    int mibNames[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, (pid_t) pid };
    size_t procInfoSize;

    if (sysctl(mibNames, sizeof(mibNames) / sizeof(mibNames[0]), nullptr, &procInfoSize, nullptr, 0) == 0) {
        kinfo_proc *procInfo = (kinfo_proc *) malloc(procInfoSize);

        if (sysctl(mibNames, sizeof(mibNames) / sizeof(mibNames[0]), procInfo, &procInfoSize, nullptr, 0) == 0)
            ppid = procInfo->kp_eproc.e_ppid;
        free(procInfo);
    }

#elif defined(Q_OS_WIN)
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(pe32);
    HANDLE hProcess = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, pid);
    if (Process32First(hProcess, &pe32)) {
        do {
            if (pe32.th32ProcessID == pid) {
                ppid = pe32.th32ParentProcessID;
                break;
            }
        } while (Process32Next(hProcess, &pe32));
    }
    CloseHandle(hProcess);
#else
    Q_UNUSED(pid)
#endif
    return ppid;
}

QT_END_NAMESPACE_AM
