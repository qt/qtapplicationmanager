/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QCoreApplication>
#include <QNetworkInterface>
#include <QPluginLoader>
#include <private/qvariant_p.h>

#include "utilities.h"
#include "exception.h"

#include <errno.h>

#if defined(Q_OS_UNIX)
#  include <unistd.h>
#endif
#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <tlhelp32.h>
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
#  include <unistd.h>
#  include <sys/mount.h>
#  include <sys/statvfs.h>
#  include <sys/sysctl.h>
#else
#  include <mntent.h>
#  include <sys/stat.h>
#  if defined(Q_OS_ANDROID)
#    include <sys/vfs.h>
#    define statvfs statfs
#  else
#    include <sys/statvfs.h>
#  endif
#  include <qplatformdefs.h>
#endif

QT_BEGIN_NAMESPACE_AM

/*! \internal
    Check a YAML document against the "standard" AM header.
    If \a numberOfDocuments is positive, the number of docs need to match exactly. If it is
    negative, the \a numberOfDocuments is taken as the required minimum amount of documents.
    Otherwise, the amount of documents is irrelevant.
*/
YamlFormat checkYamlFormat(const QVector<QVariant> &docs, int numberOfDocuments,
                           const QVector<YamlFormat> &formatTypesAndVersions) Q_DECL_NOEXCEPT_EXPR(false)
{
    int actualSize = docs.size();
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
        map.value(qSL("formatType")).toString(),
        map.value(qSL("formatVersion")).toInt()
    };

    class StringifyTypeAndVersion
    {
    public:
        StringifyTypeAndVersion() = default;
        StringifyTypeAndVersion(const QPair<QString, int> &typeAndVersion)
        {
            operator()(typeAndVersion);
        }
        QString string() const
        {
            return m_str;
        }
        void operator()(const QPair<QString, int> &typeAndVersion)
        {
            if (!m_str.isEmpty())
                m_str += qSL(" or ");
            m_str = m_str + qSL("type '") + typeAndVersion.first + qSL("', version '")
                    + QString::number(typeAndVersion.second) + qL1C('\'');
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

QMultiMap<QString, QString> mountedDirectories()
{
    QMultiMap<QString, QString> result;
#if defined(Q_OS_WIN)
    return result; // no mounts on Windows

#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    struct statfs *sfs = nullptr;
    int count = getmntinfo(&sfs, MNT_NOWAIT);

    for (int i = 0; i < count; ++i, ++sfs) {
        result.insert(QString::fromLocal8Bit(sfs->f_mntonname),
                      QString::fromLocal8Bit(sfs->f_mntfromname));
    }
#else
    FILE *pm = fopen("/proc/self/mounts", "r");
    if (!pm)
        return result;

#  if defined(Q_OS_ANDROID)
    while (struct mntent *mntPtr = getmntent(pm)) {
        result.insert(QString::fromLocal8Bit(mntPtr->mnt_dir),
                      QString::fromLocal8Bit(mntPtr->mnt_fsname));
    }
#  else
    int pathMax = static_cast<int>(pathconf("/", _PC_PATH_MAX)) * 2 + 1024;  // quite big, but better be safe than sorry
    QScopedArrayPointer<char> strBuf(new char[pathMax]);
    struct mntent mntBuf;

    while (getmntent_r(pm, &mntBuf, strBuf.data(), pathMax - 1)) {
        result.insert(QString::fromLocal8Bit(mntBuf.mnt_dir),
                      QString::fromLocal8Bit(mntBuf.mnt_fsname));
    }
#  endif
    fclose(pm);
#endif

    return result;
}

bool safeRemove(const QString &path, RecursiveOperationType type)
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

#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
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

bool recursiveOperation(const QString &path, const std::function<bool (const QString &, RecursiveOperationType)> &operation)
{
    QFileInfo pathInfo(path);

    if (pathInfo.isDir()) {
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

QVector<QObject *> loadPlugins_helper(const char *type, const QStringList &files, const char *iid) Q_DECL_NOEXCEPT_EXPR(false)
{
    QVector<QObject *> interfaces;

    try {
        for (const QString &pluginFilePath : files) {
            QPluginLoader pluginLoader(pluginFilePath);
            if (Q_UNLIKELY(!pluginLoader.load())) {
                throw Exception("could not load %1 plugin %2: %3")
                        .arg(qL1S(type)).arg(pluginFilePath, pluginLoader.errorString());
            }
            QScopedPointer<QObject> iface(pluginLoader.instance());
            if (Q_UNLIKELY(!iface || !iface->qt_metacast(iid))) {
                throw Exception("could not get an instance of '%1' from the %2 plugin %3")
                        .arg(qL1S(iid)).arg(qL1S(type)).arg(pluginFilePath);
            }
            interfaces << iface.take();
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
    std::function<void(QVariantMap *, const QVariantMap &)> recursiveMergeMap =
            [&recursiveMergeMap](QVariantMap *into, const QVariantMap &from) {
        for (auto it = from.constBegin(); it != from.constEnd(); ++it) {
            QVariant fromValue = it.value();
            QVariant &toValue = (*into)[it.key()];

            bool needsMerge = (toValue.metaType() == fromValue.metaType());

            // we're trying not to detach, so we're using v_cast to avoid copies
            if (needsMerge && (toValue.metaType() == QMetaType::fromType<QVariantMap>()))
                recursiveMergeMap(qt6_v_cast<QVariantMap>(&toValue.data_ptr()), fromValue.toMap());
            else if (needsMerge && (toValue.metaType() == QMetaType::fromType<QVariantList>()))
                into->insert(it.key(), toValue.toList() + fromValue.toList());
            else
                into->insert(it.key(), fromValue);
        }
    };
    recursiveMergeMap(&into, from);
}

QT_END_NAMESPACE_AM
