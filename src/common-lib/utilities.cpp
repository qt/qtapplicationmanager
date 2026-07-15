// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QCoreApplication>
#include <QNetworkInterface>
#include <QPluginLoader>
#include <QSet>
#include <QQmlContext>
#include <QQmlEngine>

#include "utilities.h"
#include "unix-utilities.h"
#include "exception.h"

#if defined(Q_OS_UNIX)
#  include <unistd.h>
#  include <QtCore/private/qcore_unix_p.h>
#endif
#if defined(Q_OS_LINUX)
#  include <sys/syscall.h>
#  include <sys/statfs.h>
#endif
#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <tlhelp32.h>
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#  include <unistd.h>
#  include <sys/sysctl.h>
#endif

#include <memory>

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

/*! \internal
    Check a YAML document against the "standard" AM header.
    If \a numberOfDocuments is positive, the number of docs need to match exactly. If it is
    negative, the \a numberOfDocuments is taken as the required minimum amount of documents.
    Otherwise, the amount of documents is irrelevant.
*/
YamlFormat checkYamlFormat(const QVector<QVariant> &docs, int numberOfDocuments,
                           const QVector<YamlFormat> &formatTypesAndVersions) noexcept(false)
{
    qsizetype actualSize = docs.size();
    if (actualSize < 1)
        throw Exception("no header YAML document found");

    if (numberOfDocuments < 0) {
        if (actualSize < -numberOfDocuments) {
            throw Exception("wrong number of YAML documents: expected at least %1, got %2")
                .arg(-numberOfDocuments).arg(actualSize);
        }
    } else if (numberOfDocuments > 0) {
        if (actualSize != numberOfDocuments) {
            throw Exception("wrong number of YAML documents: expected %1, got %2")
                .arg(numberOfDocuments).arg(actualSize);
        }
    }

    const auto map = docs.constFirst().toMap();
    YamlFormat actualFormatTypeAndVersion = {
        map.value(u"formatType"_s).toString(),
        map.value(u"formatVersion"_s).toInt()
    };

    class StringifyTypeAndVersion
    {
    public:
        StringifyTypeAndVersion() = default;
        StringifyTypeAndVersion(const std::pair<QString, int> &typeAndVersion)
        {
            operator()(typeAndVersion);
        }
        QString string() const
        {
            return m_str;
        }
        void operator()(const std::pair<QString, int> &typeAndVersion)
        {
            if (!m_str.isEmpty())
                m_str += u" or ";
            m_str = m_str + u"type '" + typeAndVersion.first + u"', version '"
                    + QString::number(typeAndVersion.second) + u'\'';
        }
    private:
        QString m_str;
    };

    if (!formatTypesAndVersions.contains(actualFormatTypeAndVersion)) {
        throw Exception("wrong header: expected %1, but instead got %2")
                .arg(std::for_each(formatTypesAndVersions.cbegin(), formatTypesAndVersions.cend(), StringifyTypeAndVersion()).string())
                .arg(StringifyTypeAndVersion(actualFormatTypeAndVersion).string());
    }
    return actualFormatTypeAndVersion;
}

bool safeRemove(const QString &path, RecursiveOperationType type)
{
   static const QFileDevice::Permissions ownerAccess =
           QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
           QFileDevice::ReadUser  | QFileDevice::WriteUser  | QFileDevice::ExeUser;

   switch (type) {
   case RecursiveOperationType::EnterDirectory:
       // make sure we can unlink the directory's contents
       return QFile::setPermissions(path, ownerAccess);

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

bool isPidFileSystemSupported() noexcept
{
#if defined(Q_OS_LINUX)
    static const bool result = []() {
        int self = int(::syscall(SYS_pidfd_open, ::getpid(), 0));
        if (self < 0)
            return false;
        struct ::statfs sf { };
        int r = 0;
        QT_EINTR_LOOP(r, ::fstatfs(self, &sf));
        const bool isPidFs = (r == 0) && (sf.f_type == 0x50494446 /*PID_FS_MAGIC*/);
        qt_safe_close(self);
        return isPidFs;
    }();
    return result;
#else
    return false;
#endif
}

qint64 getParentPid(qint64 pid)
{
    qint64 ppid = 0;

#if defined(Q_OS_LINUX)
    QFile f(u"/proc/%1/stat"_s.arg(pid));
    if (f.open(QIODevice::ReadOnly)) {
        // we need just the 4th field, but the 2nd is the binary name, which could be long
        QByteArray ba = f.read(512);
        // the binary name could contain ')' and/or ' ' and the kernel escapes neither...
        qsizetype pos = ba.lastIndexOf(')');
        if ((pos > 0) && (ba.length() > (pos + 5)))
            ppid = strtoll(ba.constData() + pos + 4, nullptr, 10);
    }

#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    std::array<int, 4> mib { CTL_KERN, KERN_PROC, KERN_PROC_PID, (pid_t) pid };
    kinfo_proc procInfo;
    size_t procInfoSize = sizeof(procInfo);

    if (sysctl(mib.data(), mib.size(), &procInfo, &procInfoSize, nullptr, 0) == 0)
        ppid = procInfo.kp_eproc.e_ppid;

#elif defined(Q_OS_WIN)
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(pe32);
    HANDLE hProcess = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, pid);
    if (hProcess != INVALID_HANDLE_VALUE) {
        if (Process32First(hProcess, &pe32)) {
            do {
                if ((pe32.th32ProcessID == pid) && (pe32.th32ParentProcessID != pid)) {
                    ppid = pe32.th32ParentProcessID;
                    break;
                }
            } while (Process32Next(hProcess, &pe32));
        }
        CloseHandle(hProcess);
    }
#else
    Q_UNUSED(pid)
#endif
    return ppid;
}

size_t getProcessName(qint64 pid, char *buffer, size_t bufferSize)
{
    // This function is allocation free on purpose, since it is used in signal handlers.

    if (!buffer || !bufferSize)
        return 0;

#if defined(Q_OS_LINUX)
    std::array<char, 64> procPath { };
    ::snprintf(procPath.data(), procPath.size(), "/proc/%lld/exe", static_cast<long long>(pid));
    ssize_t len = ::readlink(procPath.data(), buffer, bufferSize - 1);
    if (len < 0) {
        len = 0;

        // Plan B: pid most likely belongs to another user, so we cannot access it.
        ::snprintf(procPath.data(), procPath.size(), "/proc/%lld/comm", static_cast<long long>(pid));
        if (int fd = qt_safe_open(procPath.data(), O_RDONLY | O_CLOEXEC); fd >= 0) {
            len = qt_safe_read(fd, buffer, bufferSize - 1);
            qt_safe_close(fd);
            if ((len > 0) && (buffer[len - 1] == '\n'))
                --len; // remove trailing newline from comm
            if (len < 0)
                len = 0;
        }
    }
    buffer[len] = '\0'; // NOLINT(clang-analyzer-security.ArrayBound)
    return len;

#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    std::array<int, 4> mib { CTL_KERN, KERN_PROC, KERN_PROC_PID, (pid_t) pid };
    ::kinfo_proc procInfo;
    size_t procInfoSize = sizeof(procInfo);

    if (::sysctl(mib.data(), mib.size(), &procInfo, &procInfoSize, nullptr, 0) == 0) {
        qstrncpy(buffer, procInfo.kp_proc.p_comm, bufferSize);
        return qstrlen(buffer);
    } else {
        return 0;
    }

#else
    Q_UNUSED(pid)
    return 0;
#endif
}

int timeoutFactor()
{
    static int tf = 0;
    if (!tf) {
        tf = qMax(1, qEnvironmentVariableIntValue("AM_TIMEOUT_FACTOR"));
        if (tf > 1)
            qInfo() << "All timeouts are multiplied by" << tf << "(changed by (un)setting $AM_TIMEOUT_FACTOR)";
    }
    return tf;
}

qreal slowAnimationSpeed()
{
    return 0.2f;
}

bool recursiveOperation(const QString &path, const std::function<bool (const QString &, RecursiveOperationType)> &operation)
{
    if (path.isEmpty() || !operation)
        return false;

    QFileInfo pathInfo(path);

    // isDir() follows symlinks, so guard against attacker-controlled symlink-to-dir redirection
    if (pathInfo.isDir() && !pathInfo.isSymLink()) {
        if (!operation(path, RecursiveOperationType::EnterDirectory))
            return false;

        QDirIterator dit(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        while (dit.hasNext()) {
            dit.next();
            QFileInfo ditInfo = dit.fileInfo();

            if (ditInfo.isDir()) {
                if (!recursiveOperation(ditInfo.filePath(), operation))
                    return false;
            } else {
                if (!operation(ditInfo.filePath(), RecursiveOperationType::File))
                    return false;
            }
        }
        return operation(path, RecursiveOperationType::LeaveDirectory);
    } else {
        return operation(path, RecursiveOperationType::File);
    }
}

bool recursiveOperation(const QByteArray &path, const std::function<bool (const QString &, RecursiveOperationType)> &operation)
{
    return recursiveOperation(QString::fromLocal8Bit(path), operation);
}

bool recursiveOperation(const QDir &path, const std::function<bool (const QString &, RecursiveOperationType)> &operation)
{
    return recursiveOperation(path.absolutePath(), operation);
}

QVector<QObject *> loadPlugins_helper(const char *type, const QStringList &files, const char *iid) noexcept(false)
{
    QVector<QObject *> interfaces;
    interfaces.reserve(files.size());

    try {
        for (const QString &pluginFilePath : files) {
            QPluginLoader pluginLoader(pluginFilePath);
            if (Q_UNLIKELY(!pluginLoader.load())) {
                throw Exception("could not load %1 plugin %2: %3")
                        .arg(type).arg(pluginFilePath, pluginLoader.errorString());
            }
            std::unique_ptr<QObject> iface(pluginLoader.instance());
            if (Q_UNLIKELY(!iface || !iface->qt_metacast(iid))) {
                throw Exception("could not get an instance of '%1' from the %2 plugin %3")
                        .arg(iid).arg(type).arg(pluginFilePath);
            }
            interfaces << iface.release();
        }
    } catch (const Exception &) {
        qDeleteAll(interfaces);
        throw;
    }
    return interfaces;
}

void recursiveMergeVariantMap(QVariantMap &into, const QVariantMap &from)
{
    // no auto allowed, since this is a recursive lambda
    std::function<void(QVariantMap &, const QVariantMap &)> recursiveMergeMap =
            [&recursiveMergeMap](QVariantMap &innerInto, const QVariantMap &innerFrom) {
        for (auto it = innerFrom.constBegin(); it != innerFrom.constEnd(); ++it) {
            QVariant fromValue = it.value();
            QVariant &toValue = innerInto[it.key()];

            bool needsMerge = (toValue.metaType() == fromValue.metaType());

            // we're trying not to detach, so we're using get<> to avoid copies
            if (needsMerge && (toValue.metaType() == QMetaType::fromType<QVariantMap>()))
                recursiveMergeMap(get<QVariantMap>(toValue), fromValue.toMap());
            else if (needsMerge && (toValue.metaType() == QMetaType::fromType<QVariantList>()))
                innerInto.insert(it.key(), toValue.toList() + fromValue.toList());
            else
                innerInto.insert(it.key(), fromValue);
        }
    };
    recursiveMergeMap(into, from);
}

QString translateFromMap(const QMap<QString, QString> &languageToName, const QString &defaultName)
{
    if (!languageToName.isEmpty()) {
        QString name = languageToName.value(QLocale::system().name()); //TODO: language changes
        if (name.isNull())
            name = languageToName.value(u"en"_s);
        if (name.isNull())
            name = languageToName.value(u"en_US"_s);
        if (name.isNull())
            name = languageToName.first();
        return name;
    } else {
        return defaultName;
    }
}

void loadResource(const QString &resource) noexcept(false)
{
    QString afp = QDir().absoluteFilePath(resource);
    QStringList errors;
    QString debugSuffix;
#if defined(Q_OS_WINDOWS)
    debugSuffix = u"d"_s;
#elif defined(Q_OS_MACOS)
    debugSuffix = u"_debug"_s;
#endif

    if (QResource::registerResource(resource))
        return;
    errors.append(u"Cannot load as Qt Resource file"_s);

    QLibrary lib(afp);
    if (lib.load())
        return;
    errors.append(lib.errorString());

    if (!debugSuffix.isEmpty()) {
        QLibrary libd(afp % debugSuffix);
        if (libd.load())
            return;
        errors.append(libd.errorString());
    }
    throw Exception("Failed to load resource %1:\n  * %2").arg(resource).arg(errors.join(u"\n  * "_s));
}

void closeAndClearFileDescriptors(QVector<int> &fdList)
{
#if defined(Q_OS_UNIX)
    for (int fd : std::as_const(fdList)) {
        if (fd >= 0)
            qt_safe_close(fd);
    }
#endif
    fdList.clear();
}

void validateIdForFilesystemUsage(const QString &id)  noexcept(false)
{
    // we need to make sure that we can use the name as directory in a filesystem and inode names
    // are limited to 255 characters in Linux. We need to subtract a safety margin for prefixes
    // or suffixes though:
    static const int maxLength = 150;

    if (id.isEmpty())
        throw Exception(Error::Parse, "must not be empty");

    if (id.length() > maxLength)
        throw Exception(Error::Parse, "the maximum length is %1 characters (found %2 characters)").arg(maxLength, id.length());

    // '.' and '..' are path-traversal; '.foo' would create a hidden installation directory.
    // Reject all of them with one rule.
    if (id.startsWith(u'.'))
        throw Exception(Error::Parse, "must not start with a dot");

    // all characters need to be ASCII minus any filesystem special characters:
    bool spaceOnly = true;
    static const char *forbiddenChars = "<>:\"/\\|?*";
    for (int pos = 0; pos < id.length(); ++pos) {
        ushort ch = id.at(pos).unicode();
        if ((ch < 0x20) || (ch > 0x7f) || strchr(forbiddenChars, ch & 0xff)) {
            throw Exception(Error::Parse, "must consist of printable ASCII characters only, except any of \'%1'")
                    .arg(QString::fromLatin1(forbiddenChars));
        }
        if (spaceOnly)
            spaceOnly = QChar(ch).isSpace();
    }
    if (spaceOnly)
        throw Exception(Error::Parse, "must not consist of only white-space characters");
}

bool isDebuggerAttached(qint64 pid)
{
    if (qApp->property("_am_qmlDebugging").toBool())
        return true;

    bool debuggerAttached = false;

#if defined(Q_OS_LINUX)
    const QString procStatus = u"/proc/"_s + QString::number(pid ? pid : getpid()) + u"/status"_s;
    QFile f(procStatus);
    if (f.open(QIODevice::ReadOnly)) {
        const QByteArray data = f.readAll();
        auto pos = data.indexOf("TracerPid:\t");
        if ((pos > 0) && (data.mid(pos + 11, 2) != "0\n"))
            debuggerAttached = true;
    }

#elif defined(Q_OS_WINDOWS)
    if (pid == 0)
        debuggerAttached = IsDebuggerPresent();

#elif defined(Q_OS_MACOS)
    // Apple QA1361
    std::array<int, 4> mib { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid ? int(pid) : getpid() };
    struct kinfo_proc procInfo;
    size_t procInfoSize = sizeof(procInfo);

    procInfo.kp_proc.p_flag = 0;
    if (sysctl(mib.data(), mib.size(), &procInfo, &procInfoSize, nullptr, 0) == 0)
        debuggerAttached = (procInfo.kp_proc.p_flag & P_TRACED);
#else
    Q_UNUSED(pid)
#endif

    return debuggerAttached;
}

std::unique_ptr<QFile> openWithSafePermissions(const QString &path) noexcept(false)
{
    auto f = std::make_unique<QFile>(path);
#if defined(Q_OS_LINUX)
    if (path.startsWith(u":/")) {  // QResource paths are implicitly trusted
        if (!f->open(QFile::ReadOnly))
            throw Exception(*f, "could not open resource %1").arg(path);
        return f;
    }

    Unix::Fd fd { qt_safe_open(path.toLocal8Bit().constData(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC) };
    if (!fd)
        throw Exception(errno, "could not open %1").arg(path);

    struct ::stat st { };
    if (::fstat(fd.get(), &st) != 0)
        throw Exception(errno, "could not stat %1").arg(path);
    if (!S_ISREG(st.st_mode))
        throw Exception("%1 is not a regular file").arg(path);

    // permission checks operate on the fstat() result rather than on the path - the inode the
    // returned QFile reads from is the same one we checked here
    static const uid_t currentUser = ::getuid();
    static const gid_t currentGroup = ::getgid();

    if (st.st_mode & S_IWOTH)
        throw Exception("%1 is world-writable").arg(path);

    if ((st.st_mode & S_IWGRP) && !((st.st_gid == 0) || (st.st_gid == currentGroup))) {
        static const QSet<gid_t> currentGroups = []() {
            std::array<gid_t, NGROUPS_MAX> groupsArray;
            int groupsArraySize = ::getgroups(NGROUPS_MAX, groupsArray.data());
            if (groupsArraySize < 0)
                throw Exception("could not get the supplementary groups of the current user");
            return QSet<gid_t> { groupsArray.cbegin(), groupsArray.cbegin() + groupsArraySize };
        }();
        if (!currentGroups.contains(st.st_gid))
            throw Exception("%1 is group-writable by the unrelated group gid=%2").arg(path).arg(st.st_gid);
    }
    if ((st.st_mode & S_IWUSR) && !((st.st_uid == 0) || (st.st_uid == currentUser)))
        throw Exception("%1 is user-writable by the unrelated user uid=%2").arg(path).arg(st.st_uid);

    if (!f->open(fd.get(), QFile::ReadOnly, QFileDevice::AutoCloseHandle))
        throw Exception(*f, "could not adopt fd for %1").arg(path);
    (void) fd.release(); // NOLINT(bugprone-unused-return-value)
#else
    if (!f->open(QFile::ReadOnly))
        throw Exception(*f, "could not open %1").arg(path);
#endif
    return f;
}

#if defined(Q_OS_LINUX)
static QString s_testRootPathPrefix; // clazy:exclude=non-pod-global-static

void setTestRootPathPrefix(const QString &path)
{
    s_testRootPathPrefix = path;
}

QString testRootPathPrefix()
{
    return s_testRootPathPrefix;
}
#endif

QT_END_NAMESPACE_AM
