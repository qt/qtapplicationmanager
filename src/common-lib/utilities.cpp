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
#include <QQmlContext>
#include <QQmlEngine>
#include <qplatformdefs.h>

#include "utilities.h"
#include "exception.h"

#include <cerrno>

#if defined(Q_OS_UNIX)
#  include <unistd.h>
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
    std::function<void(QVariantMap *, const QVariantMap &)> recursiveMergeMap =
            [&recursiveMergeMap](QVariantMap *innerInto, const QVariantMap &innerFrom) {
        for (auto it = innerFrom.constBegin(); it != innerFrom.constEnd(); ++it) {
            QVariant fromValue = it.value();
            QVariant &toValue = (*innerInto)[it.key()];

            bool needsMerge = (toValue.metaType() == fromValue.metaType());

            // we're trying not to detach, so we're using v_cast to avoid copies
            if (needsMerge && (toValue.metaType() == QMetaType::fromType<QVariantMap>()))
                recursiveMergeMap(qt6_v_cast<QVariantMap>(&toValue.data_ptr()), fromValue.toMap());
            else if (needsMerge && (toValue.metaType() == QMetaType::fromType<QVariantList>()))
                innerInto->insert(it.key(), toValue.toList() + fromValue.toList());
            else
                innerInto->insert(it.key(), fromValue);
        }
    };
    recursiveMergeMap(&into, from);
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
    for (int fd : std::as_const(fdList)) {
        if (fd >= 0)
            QT_CLOSE(fd);
    }
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

    // all characters need to be ASCII minus any filesystem special characters:
    bool spaceOnly = true;
    static const char forbiddenChars[] = "<>:\"/\\|?*";
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

QT_END_NAMESPACE_AM

#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
// Debug output for std::chrono::duration is missing in Qt < 6.6
// The code below copied straight from QDebug 6.8
//TODO: remove once we don't support Qt < 6.6 anymore (most likely in 6.9)

QT_BEGIN_NAMESPACE

static QByteArray timeUnit(qint64 num, qint64 den)
{
    using namespace std::chrono;

    if (num == 1 && den > 1) {
        // sub-multiple of seconds
        char prefix = '\0';
        auto tryprefix = [&](auto d, char c) {
            static_assert(decltype(d)::num == 1, "not an SI prefix");
            if (den == decltype(d)::den)
                prefix = c;
        };

        // "u" should be "µ", but debugging output is not always UTF-8-safe
        tryprefix(std::milli{}, 'm');
        tryprefix(std::micro{}, 'u');
        tryprefix(std::nano{}, 'n');
        tryprefix(std::pico{}, 'p');
        tryprefix(std::femto{}, 'f');
        tryprefix(std::atto{}, 'a');
        // uncommon ones later
        tryprefix(std::centi{}, 'c');
        tryprefix(std::deci{}, 'd');
        if (prefix) {
            char unit[3] = { prefix, 's' };
            return QByteArray(unit, sizeof(unit) - 1);
        }
    }

    const char *unit = nullptr;
    if (num > 1 && den == 1) {
        // multiple of seconds - but we don't use SI prefixes
        auto tryunit = [&](auto d, const char *name) {
            static_assert(decltype(d)::period::den == 1, "not a multiple of a second");
            if (unit || num % decltype(d)::period::num)
                return;
            unit = name;
            num /= decltype(d)::period::num;
        };
        tryunit(hours{}, "h");
        tryunit(minutes{}, "min");
    }
    if (!unit)
        unit = "s";

    if (num == 1 && den == 1)
        return unit;
    if (Q_UNLIKELY(num < 1 || den < 1))
        return QString::asprintf("<invalid time unit %lld/%lld>", num, den).toLatin1();

    // uncommon units: will return something like "[2/3]s"
    //  strlen("[/]min") = 6
    char buf[2 * (std::numeric_limits<qint64>::digits10 + 2) + 10];
    size_t len = 0;
    auto appendChar = [&](char c) {
        Q_ASSERT(len < sizeof(buf));
        buf[len++] = c;
    };
    auto appendNumber = [&](qint64 value) {
        if (value >= 10'000 && (value % 1000) == 0)
            len += qsnprintf(buf + len, sizeof(buf) - len, "%.6g", double(value));  // "1e+06"
        else
            len += qsnprintf(buf + len, sizeof(buf) - len, "%lld", value);
    };
    appendChar('[');
    appendNumber(num);
    if (den != 1) {
        appendChar('/');
        appendNumber(den);
    }
    appendChar(']');
    memcpy(buf + len, unit, strlen(unit));
    return QByteArray(buf, len + strlen(unit));
}

void putTimeUnit(QDebug &dbg, qint64 num, qint64 den)
{
    dbg << timeUnit(num, den).constData();
}

QT_END_NAMESPACE
#endif // QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
