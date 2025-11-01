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
#include "logging.h"
#include "dbus-utilities.h"

using namespace Qt::StringLiterals;

#if defined(QT_DBUS_LIB)

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

void operator<<(QDBusArgument &arg, const DBusNull &) // ((y)y) 0x0000
{
    arg << std::tuple<std::tuple<uchar>, uchar>{ 0x00, 0x00 };
}

void operator>>(const QDBusArgument &arg, DBusNull &)
{
    std::tuple<std::tuple<uchar>, uchar> t; arg >> t;
    Q_ASSERT((t == std::tuple<std::tuple<uchar>, uchar>{ 0x00, 0x00 }));
}

class DBusInvalid
{
public:
    DBusInvalid() = default;
    DBusInvalid(const DBusInvalid &) = default;
};

void operator<<(QDBusArgument &arg, const DBusInvalid &) // (y(y)) 0xffff
{
    arg << std::tuple<uchar, std::tuple<uchar>>{ 0xff, 0xff };
}

void operator>>(const QDBusArgument &arg, DBusInvalid &)
{
    std::tuple<uchar, std::tuple<uchar>> t; arg >> t;
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


void operator<<(QDBusArgument &arg, const DBusUrl &url) // ((ay))
{
    arg << std::tuple<std::tuple<QByteArray>>{ url.m_url.toEncoded() };
}

void operator>>(const QDBusArgument &arg, DBusUrl &url)
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

QVariant convertToDBusVariant(const QVariant &variant)
{
#if !defined(QT_DBUS_LIB)
    return variant;
#else
    int type = variant.userType();

    if (type == QMetaType::UnknownType) { // JS "undefined" / CPP Invalid
        return QVariant::fromValue(DBusInvalid { });
    } else if (type == QMetaType::Nullptr) { // JS and CPP null
        return QVariant::fromValue(DBusNull { });
    } else if (type == QMetaType::QUrl) { // QtDBus does not register QUrl
        return QVariant::fromValue(DBusUrl { variant.value<QUrl>() });
#if defined(QT_QML_LIB)
    } else if (type == qMetaTypeId<QJSValue>()) {
        return convertToDBusVariant(variant.value<QJSValue>().toVariant());
#endif
    } else if (type == QMetaType::QVariant) {
        // got a matryoshka variant
        return convertToDBusVariant(variant.value<QVariant>());
    } else if (type == QMetaType::QVariantList) {
        QVariantList outList;
        const QVariantList inList = variant.toList();
        for (const auto &v : inList)
            outList.append(convertToDBusVariant(v));
        return outList;
    } else if (type == QMetaType::QVariantMap) {
        QVariantMap outMap;
        const QVariantMap inMap = variant.toMap();
        for (const auto &[k, v] : inMap.asKeyValueRange())
            outMap.insert(k, convertToDBusVariant(v));
        return outMap;
    } else {
        return variant;
    }
#endif
}

QVariant convertFromDBusVariant(const QVariant &variant)
{
#if !defined(QT_DBUS_LIB)
    return variant;
#else
    int type = variant.userType();

    if (type == qMetaTypeId<DBusInvalid>()) {
        return { };
    } else if (type == qMetaTypeId<DBusNull>()) {
        return QVariant::fromValue(nullptr);
    } else if (type == qMetaTypeId<DBusUrl>()) {
        return QUrl(variant.value<DBusUrl>().m_url);
    } else if (type == qMetaTypeId<QDBusVariant>()) {
        const auto dbusVariant = variant.value<QDBusVariant>();
        return convertFromDBusVariant(dbusVariant.variant()); // just to be on the safe side
    } else if (type == qMetaTypeId<QDBusArgument>()) {
        const auto dbusArg = variant.value<QDBusArgument>();
        switch (dbusArg.currentType()) {
        case QDBusArgument::BasicType:
        case QDBusArgument::VariantType:
            return convertFromDBusVariant(dbusArg.asVariant());
        case QDBusArgument::ArrayType: {
            QVariantList vl;
            dbusArg.beginArray();
            while (!dbusArg.atEnd()) {
                QDBusVariant elem;
                dbusArg >> elem;
                vl << convertFromDBusVariant(elem.variant());
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

                vm.insert(key, convertFromDBusVariant(value.variant()));
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
                        return convertFromDBusVariant(variant);
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
            outList.append(convertFromDBusVariant(v));
        return outList;
    } else if (type == QMetaType::QVariantMap) {
        QVariantMap outMap;
        const QVariantMap inMap = variant.toMap();
        for (const auto &[k, v] : inMap.asKeyValueRange())
            outMap.insert(k, convertFromDBusVariant(v));
        return outMap;
    } else {
        return variant;
    }
#endif
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

QT_END_NAMESPACE_AM
