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


QT_BEGIN_NAMESPACE_AM

QVariant convertToDBusVariant(const QVariant &variant)
{
#if !defined(QT_QML_LIB)
    return variant;
#else
    int type = variant.userType();

    if (type == qMetaTypeId<QJSValue>()) {
        return convertToDBusVariant(variant.value<QJSValue>().toVariant());
    } else if (type == QMetaType::QUrl) {
        return QVariant(variant.toUrl().toString());
    } else if (type == QMetaType::QVariant) {
        // got a matryoshka variant
        return convertToDBusVariant(variant.value<QVariant>());
    } else if ((type == QMetaType::UnknownType) || (type == QMetaType::Nullptr)) {
        // we cannot send QVariant::Invalid and null values via DBus, so we abuse BYTE(0) for this purpose
        return QVariant::fromValue<uchar>(0);
    } else if (type == QMetaType::QVariantList) {
        QVariantList outList;
        QVariantList inList = variant.toList();
        for (auto it = inList.cbegin(); it != inList.cend(); ++it)
            outList.append(convertToDBusVariant(*it));
        return outList;
    } else if (type == QMetaType::QVariantMap) {
        QVariantMap outMap;
        QVariantMap inMap = variant.toMap();
        for (auto it = inMap.cbegin(); it != inMap.cend(); ++it)
            outMap.insert(it.key(), convertToDBusVariant(it.value()));
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

    if (type == QMetaType::UChar && variant.value<uchar>() == 0) {
        // we cannot send QVariant::Invalid via DBus, so we abuse BYTE(0) for this purpose
        return QVariant();
    } else if (type == qMetaTypeId<QDBusVariant>()) {
        QDBusVariant dbusVariant = variant.value<QDBusVariant>();
        return convertFromDBusVariant(dbusVariant.variant()); // just to be on the safe side
    } else if (type == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument dbusArg = variant.value<QDBusArgument>();
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
        default:
            return QVariant();
        }
    } else if (type == QMetaType::QVariantList) {
        QVariantList outList;
        QVariantList inList = variant.toList();
        for (auto it = inList.cbegin(); it != inList.cend(); ++it)
            outList.append(convertFromDBusVariant(*it));
        return outList;
    } else if (type == QMetaType::QVariantMap) {
        QVariantMap outMap;
        QVariantMap inMap = variant.toMap();
        for (auto it = inMap.cbegin(); it != inMap.cend(); ++it)
            outMap.insert(it.key(), convertFromDBusVariant(it.value()));
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
        qDBusRegisterMetaType<QUrl>();
        qDBusRegisterMetaType<QMap<QString, QDBusUnixFileDescriptor>>();
        qDBusRegisterMetaType<QtAM::UnixFdMap>();
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

#if defined(QT_DBUS_LIB)
QT_BEGIN_NAMESPACE

QDBusArgument &operator<<(QDBusArgument &argument, const QUrl &url)
{
    argument.beginStructure();
    argument << QString::fromLatin1(url.toEncoded());
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, QUrl &url)
{
    QString s;
    argument.beginStructure();
    argument >> s;
    argument.endStructure();
    url = QUrl::fromEncoded(s.toLatin1());
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const QtAM::UnixFdMap &fdMap)
{
    argument.beginMap(qMetaTypeId<QString>(), qMetaTypeId<QDBusUnixFileDescriptor>());
    for (auto it = fdMap.cbegin(); it != fdMap.cend(); ++it) {
        argument.beginMapEntry();
        argument << it.key();
        argument << it.value();
        argument.endMapEntry();
    }
    argument.endMap();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, QtAM::UnixFdMap &fdMap)
{
    argument.beginMap();
    fdMap.clear();
    while (!argument.atEnd()) {
        QString key;
        QDBusUnixFileDescriptor value;
        argument.beginMapEntry();
        argument >> key >> value;
        argument.endMapEntry();
        fdMap.insert(key, value);
    }
    argument.endMap();
    return argument;
}

QT_END_NAMESPACE
#endif
