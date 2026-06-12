// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QUrl>
#if defined(QT_QML_LIB)
#  include <QJSValue>
#endif
#if defined(QT_DBUS_LIB)
#  include <QDBusVariant>
#  include <QDBusArgument>
#  include <QDBusMetaType>
#  include <QDBusUnixFileDescriptor>
#  include <QDBusConnection>
#  include <QLibrary>
#  include <QLibraryInfo>
#  include <QDir>
#endif
#if defined(Q_OS_WIN)
#  include <windows.h>
#  ifdef interface
#    undef interface
#  endif
#endif
#if defined(Q_OS_LINUX)
#  include <dlfcn.h>
#  include <sys/socket.h>
#  include "utilities.h"
#endif
#include "logging.h"
#include "dbus-utilities.h"

using namespace Qt::StringLiterals;

#if defined(QT_DBUS_LIB)

#  if QT_VERSION < QT_VERSION_CHECK(6, 10, 0)
template <typename... T>
[[maybe_unused]] QDBusArgument &operator<<(QDBusArgument &argument, const std::tuple<T...> &tuple)
{
    static_assert(sizeof...(T) != 0, "D-Bus doesn't allow empty structs");
    argument.beginStructure();
    std::apply([&argument](const auto &...elements) { (argument << ... << elements); }, tuple);
    argument.endStructure();
    return argument;
}

template <typename... T>
[[maybe_unused]] const QDBusArgument &operator>>(const QDBusArgument &argument, std::tuple<T...> &tuple)
{
    static_assert(sizeof...(T) != 0, "D-Bus doesn't allow empty structs");
    argument.beginStructure();
    std::apply([&argument](auto &...elements) { (argument >> ... >> elements); }, tuple);
    argument.endStructure();
    return argument;
}
#  endif

namespace {

// QVariant() [undefined] and QVariant(nullptr) [null] are common, but cannot be mapped to any
// standard D-Bus type. We did abuse uchar(0) for null in the past, but using dedicated types is
// cleaner and less error prone.

class DBusNull
{
public:
    DBusNull() = default;
    DBusNull(const DBusNull &) = default;
};

[[maybe_unused]] void operator<<(QDBusArgument &arg, const DBusNull &) // ((y)y) 0x0000
{
    arg << std::tuple<std::tuple<uchar>, uchar>{ 0x00, 0x00 };
}

[[maybe_unused]] void operator>>(const QDBusArgument &arg, DBusNull &)
{
    std::tuple<std::tuple<uchar>, uchar> t; arg >> t;

    // This assert is a debug-only collision tripwire: D-Bus signatures encode structure
    // but not type identity, so any third-party type marshalling to the same signature would also
    // reach this demarshaller. Asserting the byte-value convention catches such collisions during
    // development; in production they're benign (the wrong type just decodes as null/invalid).
    Q_ASSERT((t == std::tuple<std::tuple<uchar>, uchar>{0x00, 0x00}));
}

class DBusInvalid
{
public:
    DBusInvalid() = default;
    DBusInvalid(const DBusInvalid &) = default;
};

[[maybe_unused]] void operator<<(QDBusArgument &arg, const DBusInvalid &) // (y(y)) 0xffff
{
    arg << std::tuple<uchar, std::tuple<uchar>>{ 0xff, 0xff };
}

[[maybe_unused]] void operator>>(const QDBusArgument &arg, DBusInvalid &)
{
    std::tuple<uchar, std::tuple<uchar>> t; arg >> t;

    // see DBusNull's operator>> for the rationale behind this assert
    Q_ASSERT((t == std::tuple<uchar, std::tuple<uchar>>{ 0xff, 0xff }));
}

// QUrl is also common and has no native D-Bus type. We map it to a byte-array inside a

// structure, inside a structure and store the encoded URL there.
// We could register QUrl directly, but this could interfere with user code that registers QUrl
// as well.

class DBusUrl
{
public:
    DBusUrl() = default;
    DBusUrl(const QUrl &url) : m_url(url) { }

    QUrl m_url;
};


[[maybe_unused]] void operator<<(QDBusArgument &arg, const DBusUrl &url) // ((ay))
{
    arg << std::tuple<std::tuple<QByteArray>>{ url.m_url.toEncoded() };
}

[[maybe_unused]] void operator>>(const QDBusArgument &arg, DBusUrl &url)
{
    std::tuple<std::tuple<QByteArray>> t; arg >> t;
    url.m_url = QUrl::fromEncoded(std::get<0>(std::get<0>(t)));
}

} // anonymous namespace

Q_DECLARE_METATYPE(DBusNull)
Q_DECLARE_METATYPE(DBusInvalid)
Q_DECLARE_METATYPE(DBusUrl)

#endif // QT_DBUS_LIB

QT_BEGIN_NAMESPACE_AM

// guard against attacker-shaped variants nested deeply enough to blow the stack
static constexpr int MaxConversionDepth = 64;

static QVariant convertToDBusVariantImpl(const QVariant &variant, int depth)
{
#if !defined(QT_DBUS_LIB)
    Q_UNUSED(depth)
    Q_UNUSED(MaxConversionDepth)
    return variant;
#else
    if (depth >= MaxConversionDepth) {
        qCCritical(LogDBus) << "convertToDBusVariant: nesting level exceeds" << MaxConversionDepth
                            << "- returning empty variant";
        return { };
    }
    int type = variant.userType();

    if (type == QMetaType::UnknownType) { // JS "undefined" / CPP Invalid
        return QVariant::fromValue(DBusInvalid { });
    } else if (type == QMetaType::Nullptr) { // JS and CPP null
        return QVariant::fromValue(DBusNull { });
    } else if (type == QMetaType::QUrl) { // QtDBus does not register QUrl
        return QVariant::fromValue(DBusUrl { variant.value<QUrl>() });
#if defined(QT_QML_LIB)
    } else if (type == qMetaTypeId<QJSValue>()) {
        return convertToDBusVariantImpl(variant.value<QJSValue>().toVariant(), depth + 1);
#endif
    } else if (type == QMetaType::QVariant) {
        // got a matryoshka variant
        return convertToDBusVariantImpl(variant.value<QVariant>(), depth + 1);
    } else if (type == QMetaType::QVariantList) {
        QVariantList outList;
        const QVariantList inList = variant.toList();
        for (const auto &v : inList)
            outList.append(convertToDBusVariantImpl(v, depth + 1));
        return outList;
    } else if (type == QMetaType::QVariantMap) {
        QVariantMap outMap;
        const QVariantMap inMap = variant.toMap();
        for (const auto &[k, v] : inMap.asKeyValueRange())
            outMap.insert(k, convertToDBusVariantImpl(v, depth + 1));
        return outMap;
    } else {
        return variant;
    }
#endif
}

QVariant convertToDBusVariant(const QVariant &variant)
{
    return convertToDBusVariantImpl(variant, 0);
}

static QVariant convertFromDBusVariantImpl(const QVariant &variant, int depth)
{
#if !defined(QT_DBUS_LIB)
    Q_UNUSED(MaxConversionDepth)
    Q_UNUSED(depth)
    return variant;
#else
    if (depth >= MaxConversionDepth) {
        qCWarning(LogDBus) << "convertFromDBusVariant: nesting level exceeds" << MaxConversionDepth
                           << "- returning empty variant";
        return {};
    }
    int type = variant.userType();

    if (type == qMetaTypeId<DBusInvalid>()) {
        return { };
    } else if (type == qMetaTypeId<DBusNull>()) {
        return QVariant::fromValue(nullptr);
    } else if (type == qMetaTypeId<DBusUrl>()) {
        return QUrl(variant.value<DBusUrl>().m_url);
    } else if (type == qMetaTypeId<QDBusVariant>()) {
        const auto dbusVariant = variant.value<QDBusVariant>();
        return convertFromDBusVariantImpl(dbusVariant.variant(), depth + 1); // just to be on the safe side
    } else if (type == qMetaTypeId<QDBusArgument>()) {
        const auto dbusArg = variant.value<QDBusArgument>();
        switch (dbusArg.currentType()) {
        case QDBusArgument::BasicType:
        case QDBusArgument::VariantType:
            return convertFromDBusVariantImpl(dbusArg.asVariant(), depth + 1);
        case QDBusArgument::ArrayType: {
            QVariantList vl;
            dbusArg.beginArray();
            while (!dbusArg.atEnd()) {
                QDBusVariant elem;
                dbusArg >> elem;
                vl << convertFromDBusVariantImpl(elem.variant(), depth + 1);
            }
            dbusArg.endArray();
            return vl;
        }
        case QDBusArgument::MapType: {
            QVariantMap vm;
            dbusArg.beginMap();
            while (!dbusArg.atEnd()) {
                dbusArg.beginMapEntry();
                QString key;
                QDBusVariant value;
                dbusArg >> key >> value;
                dbusArg.endMapEntry();

                vm.insert(key, convertFromDBusVariantImpl(value.variant(), depth + 1));
            }
            dbusArg.endMap();
            return vm;
        }
        case QDBusArgument::StructureType: {
            // this is really stupid, but QDBusMetaType::signatureToType() does not work for
            // complex types, although it has all the information (in a private list...)
            const auto sig = dbusArg.currentSignature();
            for (int meta : { qMetaTypeId<DBusNull>(), qMetaTypeId<DBusInvalid>(),
                             qMetaTypeId<DBusUrl>(),
                             int(QMetaType::QDate), int(QMetaType::QTime), int(QMetaType::QDateTime),
                             int(QMetaType::QRect), int(QMetaType::QRectF),
                             int(QMetaType::QSize), int(QMetaType::QSizeF),
                             int(QMetaType::QPoint), int(QMetaType::QPointF),
                             int(QMetaType::QLine), int(QMetaType::QLineF) }) {
                auto metaSig = QString::fromLatin1(QDBusMetaType::typeToSignature(QMetaType(meta)));
                if (sig == metaSig) {
                    auto variant = QVariant::fromMetaType(QMetaType { meta });
                    if (QDBusMetaType::demarshall(dbusArg, QMetaType { meta }, variant.data()))
                        return convertFromDBusVariantImpl(variant, depth + 1);
                }
            }
            Q_FALLTHROUGH();
        }
        default:
            return { };
        }
    } else if (type == QMetaType::QVariantList) {
        QVariantList outList;
        const QVariantList inList = variant.toList();
        for (const auto &v : inList)
            outList.append(convertFromDBusVariantImpl(v, depth + 1));
        return outList;
    } else if (type == QMetaType::QVariantMap) {
        QVariantMap outMap;
        const QVariantMap inMap = variant.toMap();
        for (const auto &[k, v] : inMap.asKeyValueRange())
            outMap.insert(k, convertFromDBusVariantImpl(v, depth + 1));
        return outMap;
    } else {
        return variant;
    }
#endif
}

QVariant convertFromDBusVariant(const QVariant &variant)
{
    return convertFromDBusVariantImpl(variant, 0);
}

void registerDBusTypes()
{
#if defined(QT_DBUS_LIB)
    static bool once = false;
    if (!once) {
        qDBusRegisterMetaType<QMap<QString, QDBusUnixFileDescriptor>>();
        qDBusRegisterMetaType<DBusInvalid>();
        qDBusRegisterMetaType<DBusNull>();
        qDBusRegisterMetaType<DBusUrl>();

        once = true;
    }
#endif
}

void ensureLibDBusIsAvailable()
{
#if (defined(Q_OS_WINDOWS) || defined(Q_OS_MACOS)) && defined(QT_DBUS_LIB)
    // On Windows and macOS, libdbus-1 is not readily available, but we need it to communicate
    // between appman and appman-controller.
    // We first check if the user has a custom libdbus-1 installed already. If not, we load the
    // one that comes with the application manager.
#  if defined(Q_OS_WINDOWS)
    static const QString dbusLibName = u"dbus-1"_s;
    auto dbusLoadPrepare = []() {
        const QString dllPath = QLibraryInfo::path(QLibraryInfo::BinariesPath)
                                + u"/qtapplicationmanager";
        ::SetDllDirectoryW((LPCWSTR) dllPath.utf16());
    };
    auto dbusLoadCleanup = []() {
        ::SetDllDirectoryW(nullptr);
    };

#  elif defined(Q_OS_MACOS)
    static const QString dbusLibName = u"libdbus-1"_s;
    QString currentPath;
    auto dbusLoadPrepare = [&currentPath]() {
        const QString dylibPath = QLibraryInfo::path(QLibraryInfo::LibrariesPath)
                                  + u"/qtapplicationmanager";
        // adding to DYLD_LIBRARY_PATH has no effect on the running process
        currentPath = QDir::currentPath();
        QDir::setCurrent(dylibPath);
    };
    auto dbusLoadCleanup = [&currentPath]() {
        QDir::setCurrent(currentPath);
    };
#  endif

    static QLibrary dbusLib(dbusLibName);
    if (!dbusLib.isLoaded() && !dbusLib.load()) {
        dbusLoadPrepare();

        if (!dbusLib.load() || !dbusLib.resolve("dbus_connection_open_private"))
            qCCritical(LogDBus) << "WARNING: could not load the application manager's libdbus-1 for appman-controller support.";
        else
            qCInfo(LogDBus) << "Loaded the application manager's libdbus-1 for appman-controller support.";
        dbusLoadCleanup();
    }
#endif
}

QString escapeDBusAddressName(const QString &name)
{
    QByteArray escaped;
    const QByteArray unescaped = name.toUtf8();
    for (const char c : unescaped) {
        if (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9'))
            || (c == '-') || (c == '_') || (c == '/') || (c == '\\') || (c == '*') || (c == '.')) {
            escaped += c;
        } else {
            escaped = escaped + '%' + QByteArray(1, c).toHex(c);
        }
    }
    return QString::fromLatin1(escaped);
}

#if defined(Q_OS_LINUX) && defined(QT_DBUS_LIB)

std::pair<qint64, Unix::Fd> getDBusPeerPidAndFd(const QDBusConnection &conn)
{
    using am_dbus_connection_get_socket_t = bool (*)(void *, int *);
    static am_dbus_connection_get_socket_t am_dbus_connection_get_socket = nullptr;

    if (!am_dbus_connection_get_socket) {
        am_dbus_connection_get_socket = reinterpret_cast<am_dbus_connection_get_socket_t>(
            dlsym(RTLD_DEFAULT, "dbus_connection_get_socket"));
    }

    if (!am_dbus_connection_get_socket)
        qFatal("ERROR: could not resolve 'dbus_connection_get_socket' from libdbus-1");

    int socketFd = -1;
    if (am_dbus_connection_get_socket(conn.internalPointer(), &socketFd)) {
        struct ::ucred ucred;
        socklen_t ucredSize = sizeof(struct ::ucred);
        if (::getsockopt(socketFd, SOL_SOCKET, SO_PEERCRED, &ucred, &ucredSize) == 0) {
            int pidfd = -1;

#if defined(SO_PEERPIDFD)
            if (isPidFileSystemSupported()) {
                socklen_t pidfdSize = sizeof(pidfd);
                ::getsockopt(socketFd, SOL_SOCKET, SO_PEERPIDFD, &pidfd, &pidfdSize);
            }
#endif
            return { ucred.pid, Unix::Fd(pidfd) };
        }
    }
    return { 0, Unix::Fd() };
}

#endif

QT_END_NAMESPACE_AM
