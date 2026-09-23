// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QFile>
#include <QFileInfo>
#include <QDir>
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
#  include <fcntl.h>
#  include <sys/stat.h>
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
#include <optional>
#include <vector>

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

std::optional<std::tuple<QString, QString>> sanitizeAsDirAndEntry(const QString &path)
{
    QString p = path;
    while (p.endsWith(u'/'))
        p.chop(1);
    const qsizetype slash = p.lastIndexOf(u'/');
    QString entry = p.mid(slash + 1);
    if (entry.isEmpty() || (entry == u".") || (entry == u"..")) // "", "/", ".", ".."
        return std::nullopt;
    QString dir = (slash < 0) ? u"."_s : ((slash == 0) ? u"/"_s : p.left(slash));
    return std::make_tuple(std::move(dir), std::move(entry));
}

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

// Opens the directory entry below dirFd for reading. The owner may lack the r bit (e.g. 0300
// or 0000): on EACCES, add rwx and retry. Returns an invalid fd with errno set on failure;
// ELOOP/ENOTDIR mean the entry is not a directory (anymore).
static Unix::Fd openDirectory(int dirFd, const char *entry)
{
    int fd = -1;
    QT_EINTR_LOOP(fd, ::openat(dirFd, entry, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if ((fd >= 0) || (errno != EACCES))
        return Unix::Fd(fd);

    // Pin the directory with O_PATH (needs no permission on it), chmod it through its /proc link
    // and re-open through the pinned fd: no name is looked up twice, so nothing can be swapped in.
    // glibc before 2.32 returns ENOTSUP for fchmodat(AT_SYMLINK_NOFOLLOW). Newer versions emulate
    // it through /proc exactly like this. Doing it ourselves drops the libc dependency and lets us
    // re-open through the pinned fd instead of by name.
    int pathFd = -1;
    QT_EINTR_LOOP(pathFd, ::openat(dirFd, entry, O_PATH | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    Unix::Fd pinned(pathFd);
    if (!pinned)
        return { };
    struct ::stat st { };
    if (::fstat(pinned.get(), &st) != 0)
        return { };
    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return { };
    }
    std::array<char, 32> procPath { };
    ::snprintf(procPath.data(), procPath.size(), "/proc/self/fd/%d", pinned.get());
    // Just 'r' would be enough for openat to succeed, but we need 'rwx' anyway for the subsequent
    // removal of the directory contents, so we just do it in one step here.
    if (::chmod(procPath.data(), (st.st_mode & ~S_IFMT) | S_IRWXU) != 0) {
        errno = EACCES; // not the owner, or no /proc: report the original failure
        return { };
    }
    QT_EINTR_LOOP(fd, ::openat(pinned.get(), ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    return Unix::Fd(fd);
}

// Path-based walks would implicitly limit the recursion depth due to PATH_MAX, but our
// fd-relative walk can not, so we need to bound it explicitly.
static constexpr int MaxDirectoryDepth = 512;

static int removeDirectoryContents(const Unix::Fd &dirFd, dev_t rootDev, int depth);

// Removes the entry inside dirFd, recursively if it is a directory. Returns 0 or an errno.
static int removeEntry(int dirFd, const char *entry, std::optional<dev_t> rootDev, int depth)
{
    struct ::stat st { };
    if (::fstatat(dirFd, entry, &st, AT_SYMLINK_NOFOLLOW) != 0)
        return errno;
    // regular file, symlink (also dangling), socket, ...: unlinkat() never follows
    if (!S_ISDIR(st.st_mode))
        return (::unlinkat(dirFd, entry, 0) == 0) ? 0 : errno;

    Unix::Fd dir = openDirectory(dirFd, entry);
    if (!dir) {
        // swapped for a symlink or a non-directory since fstatat(): unlink the entry itself
        if ((errno == ELOOP) || (errno == ENOTDIR))
            return (::unlinkat(dirFd, entry, 0) == 0) ? 0 : errno;
        return errno;
    }
    if (int e = removeDirectoryContents(dir, rootDev.value_or(st.st_dev), depth); e != 0)
        return e;
    return (::unlinkat(dirFd, entry, AT_REMOVEDIR) == 0) ? 0 : errno;
}

// Empties the directory behind dirFd (opened with O_DIRECTORY | O_NOFOLLOW), but does not remove
// it. All decisions are made on descriptors, never on re-resolved paths. Returns 0 or an errno.
static int removeDirectoryContents(const Unix::Fd &dirFd, dev_t rootDev, int depth)
{
    if (depth >= MaxDirectoryDepth)
        return ENAMETOOLONG;

    struct ::stat st { };
    if (::fstat(dirFd.get(), &st) != 0) // is the directory accessible?
        return errno;
    if (st.st_dev != rootDev) // are we still on the same filesystem?
        return EXDEV;
    // The owner needs wx on the directory to unlink entries.
    // This complements the openDirectory() logic above for cases where we were able to open
    // the directory ('r' bit set) but we're now lacking 'wx'.
    if ((st.st_mode & (S_IWUSR | S_IXUSR)) != (S_IWUSR | S_IXUSR)) {
        if (::fchmod(dirFd.get(), (st.st_mode & ~S_IFMT) | S_IRWXU) != 0)
            return errno;
    }

    // snapshot the names first: unlinking while readdir() is running is unspecified by POSIX
    QList<QByteArray> names;
    {
        Unix::Dir dir(dirFd.duplicate());
        if (!dir)
            return errno;
        auto optionalNames = dir.entryNames();
        if (!optionalNames)
            return errno;
        names = *optionalNames;
    }

    for (const QByteArray &name : std::as_const(names)) {
        int e = removeEntry(dirFd.get(), name.constData(), rootDev, depth + 1);
        // an entry that vanished in the meantime (e.g. concurrent cleanup) is fine with us
        if ((e != 0) && (e != ENOENT))
            return e;
    }
    return 0;
}

bool removeRecursively(int dirFd, const QByteArray &entry)
{
    if (entry.isEmpty() || entry.contains('/') || (entry == ".") || (entry == "..")) {
        errno = EINVAL;
        return false;
    }
    errno = removeEntry(dirFd, entry.constData(), std::nullopt, 0);
    return errno == 0;
}

bool removeRecursively(const QString &path)
{
    const auto dirAndEntry = sanitizeAsDirAndEntry(path);
    if (!dirAndEntry) {
        errno = EINVAL;
        return false;
    }
    const auto &[dir, entry] = *dirAndEntry;

    // resolve the parent once (following symlinks above the dir, like any path-taking API) and
    // act on the entry relative to it
    int e = 0;
    {
        Unix::Fd dirFd { qt_safe_open(QFile::encodeName(dir).constData(),
                                      O_RDONLY | O_DIRECTORY | O_CLOEXEC) };
        if (!dirFd)
            return false;
        e = removeEntry(dirFd.get(), QFile::encodeName(entry).constData(), std::nullopt, 0);
    }
    errno = e;
    return e == 0;
}

#else // !Q_OS_LINUX || Q_OS_ANDROID

bool removeRecursively(const QString &path)
{
    if (!sanitizeAsDirAndEntry(path)) {
        errno = EINVAL;
        return false;
    }
    // no privileged helper on these platforms; QDir also copes with Windows' read-only attribute
    QFileInfo fi(path);
    if (!fi.exists() && !fi.isSymLink()) {
        errno = ENOENT;
        return false;
    }
    const bool ok = (fi.isDir() && !fi.isSymLink()) ? QDir(path).removeRecursively()
                                                    : QFile::remove(path);
    if (!ok)
        errno = EIO; // QDir and QFile do not report why
    return ok;
}

#endif // Q_OS_LINUX && !Q_OS_ANDROID

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
