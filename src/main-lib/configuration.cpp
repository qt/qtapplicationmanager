/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
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

#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QDataStream>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QtConcurrent/QtConcurrent>

#include <functional>

#include "global.h"
#include "logging.h"
#include "qtyaml.h"
#include "utilities.h"
#include "exception.h"
#include "qml-utilities.h"
#include "configuration.h"

// enable this to benchmark the config cache
//#define AM_TIME_CONFIG_PARSING

// use QtConcurrent to parse the config files, if there are more than x config files
#define AM_PARALLEL_THRESHOLD  1

QT_BEGIN_NAMESPACE_AM


template<> bool Configuration::value(const char *clname, const QVector<const char *> &cfname) const
{
    return (clname && m_clp.isSet(qL1S(clname))) || findInConfigFile(cfname).toBool();
}

template<> QString Configuration::value(const char *clname, const QVector<const char *> &cfname) const
{
    QString clval;
    if (clname)
        clval = m_clp.value(qL1S(clname));
    bool cffound;
    QString cfval = findInConfigFile(cfname, &cffound).toString();
    return ((clname && m_clp.isSet(qL1S(clname))) || !cffound) ? clval : cfval;
}

template<> QStringList Configuration::value(const char *clname, const QVector<const char *> &cfname) const
{
    QStringList result;
    if (clname)
        result = m_clp.values(qL1S(clname));
    if (!cfname.isEmpty())
        result += variantToStringList(findInConfigFile(cfname));
    return result;
}

template<> QVariant Configuration::value(const char *clname, const QVector<const char *> &cfname) const
{
    QString yaml;
    if (clname)
        yaml = m_clp.value(qL1S(clname));
    if (!yaml.isEmpty()) {
        auto docs = QtYaml::variantDocumentsFromYaml(yaml.toUtf8());
        return docs.isEmpty() ? QVariant() : docs.constFirst();
    } else {
        return findInConfigFile(cfname);
    }
}

QVariant Configuration::findInConfigFile(const QVector<const char *> &path, bool *found) const
{
    if (found)
        *found = false;

    if (path.isEmpty())
        return QVariant();

    QVariantMap var = m_config;

    for (int i = 0; i < (path.size() - 1); ++i) {
        QVariant subvar = var.value(qL1S(path.at(i)));
        if (subvar.type() == QVariant::Map)
            var = subvar.toMap();
        else
            return QVariant();
    }
    if (found)
        *found = var.contains(qL1S(path.last()));
    return var.value(qL1S(path.last()));
}

void Configuration::mergeConfig(const QVariantMap &other)
{
    recursiveMergeVariantMap(m_config, other);
}


Configuration::Configuration(const QStringList &defaultConfigFilePaths, const QString &buildConfigFilePath)
    : m_defaultConfigFilePaths(defaultConfigFilePaths)
    , m_buildConfigFilePath(buildConfigFilePath)
{
    m_clp.addHelpOption();
    m_clp.addVersionOption();
    QCommandLineOption cf { { qSL("c"), qSL("config-file") }, qSL("load cnfiguration from file (can be given multiple times)."), qSL("files") };
    cf.setDefaultValues(m_defaultConfigFilePaths);
    m_clp.addOption(cf);
    m_clp.addOption({ { qSL("o"), qSL("option") }, qSL("override a specific config option."), qSL("yaml-snippet") });
    m_clp.addOption({ qSL("no-config-cache"),      qSL("disable the use of the config file cache.") });
    m_clp.addOption({ qSL("clear-config-cache"),   qSL("ignore an existing config file cache.") });
    if (!buildConfigFilePath.isEmpty())
        m_clp.addOption({ qSL("build-config"),         qSL("dumps the build configuration and exits.") });
}

QVariant Configuration::buildConfig() const
{
    QFile f(m_buildConfigFilePath);
    if (f.open(QFile::ReadOnly))
        return QtYaml::variantDocumentsFromYaml(f.readAll()).toList();
    else
        return QVariant();
}

Configuration::~Configuration()
{

}

// vvvv copied from QCommandLineParser ... why is this not public API?

#if defined(Q_OS_ANDROID)
#  include <android/log.h>
#elif defined(Q_OS_WIN) && !defined(Q_OS_WINCE) && !defined(Q_OS_WINRT)
#  include <windows.h>

// Return whether to use a message box. Use handles if a console can be obtained
// or we are run with redirected handles (for example, by QProcess).
static inline bool displayMessageBox()
{
    if (GetConsoleWindow())
        return false;
    STARTUPINFO startupInfo;
    startupInfo.cb = sizeof(STARTUPINFO);
    GetStartupInfo(&startupInfo);
    return !(startupInfo.dwFlags & STARTF_USESTDHANDLES);
}
#endif // Q_OS_WIN && !QT_BOOTSTRAPPED && !Q_OS_WIN && !Q_OS_WINRT

void Configuration::showParserMessage(const QString &message, MessageType type)
{
#if defined(Q_OS_WIN) && !defined(Q_OS_WINCE) && !defined(Q_OS_WINRT)
    if (displayMessageBox()) {
        const UINT flags = MB_OK | MB_TOPMOST | MB_SETFOREGROUND
            | (type == UsageMessage ? MB_ICONINFORMATION : MB_ICONERROR);
        QString title;
        if (QCoreApplication::instance())
            title = QCoreApplication::instance()->property("applicationDisplayName").toString();
        if (title.isEmpty())
            title = QCoreApplication::applicationName();
        MessageBoxW(0, reinterpret_cast<const wchar_t *>(message.utf16()),
                    reinterpret_cast<const wchar_t *>(title.utf16()), flags);
        return;
    }
#elif defined(Q_OS_ANDROID)
    static QByteArray appName = QCoreApplication::applicationName().toLocal8Bit();

    __android_log_write(type == UsageMessage ? ANDROID_LOG_WARN : ANDROID_LOG_ERROR,
                        appName.constData(), qPrintable(message));
    return;

#endif // Q_OS_WIN && !QT_BOOTSTRAPPED && !Q_OS_WIN && !Q_OS_WINRT
    fputs(qPrintable(message), type == UsageMessage ? stdout : stderr);
}

// ^^^^ copied from QCommandLineParser ... why is this not public API?

void Configuration::parse(QStringList *deploymentWarnings)
{
    if (!m_clp.parse(QCoreApplication::arguments())) {
        showParserMessage(m_clp.errorText() + qL1C('\n'), ErrorMessage);
        exit(1);
    }

    if (m_clp.isSet(qSL("version")))
        m_clp.showVersion();

    if (m_clp.isSet(qSL("help")))
        m_clp.showHelp();

    if (!m_buildConfigFilePath.isEmpty() && m_clp.isSet(qSL("build-config"))) {
        QFile f(m_buildConfigFilePath);
        if (f.open(QFile::ReadOnly)) {
            showParserMessage(QString::fromLocal8Bit(f.readAll()), UsageMessage);
            exit(0);
        } else {
            showParserMessage(qL1S("Could not find the embedded build config.\n"), ErrorMessage);
            exit(1);
        }
    }
#if defined(AM_TIME_CONFIG_PARSING)
    QElapsedTimer timer;
    timer.start();
#endif

    QStringList configFilePaths = m_clp.values(qSL("config-file"));

    struct ConfigFile
    {
        QString filePath;    // abs. file path
        QByteArray checksum; // sha1 (fast and sufficient for this use-case)
        QByteArray content;
        QVariantMap config;
    };
    QVarLengthArray<ConfigFile> configFiles(configFilePaths.size());

    for (int i = 0; i < configFiles.size(); ++i)
        configFiles[i].filePath = QFileInfo(configFilePaths.at(i)).absoluteFilePath();

    bool noConfigCache = m_clp.isSet(qSL("no-config-cache"));
    bool clearConfigCache = m_clp.isSet(qSL("clear-config-cache"));

    const QDir cacheLocation = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);

    if (!cacheLocation.exists())
        cacheLocation.mkpath(qSL("."));

    const QString cacheFilePath = cacheLocation.absoluteFilePath(qSL("appman-config.cache"));

    QFile cacheFile(cacheFilePath);
    QAtomicInt useCache = false;
    QVariantMap cache;

    if (!noConfigCache && !clearConfigCache) {
        if (cacheFile.open(QFile::ReadOnly)) {
            try {
                QDataStream ds(&cacheFile);
                QVector<QPair<QString, QByteArray>> configChecksums; // abs. file path -> sha1
                ds >> configChecksums >> cache;

                if (ds.status() != QDataStream::Ok)
                    throw Exception("failed to read config cache content");

                if (configFiles.count() != configChecksums.count())
                    throw Exception("the number of cached config files does not match the current set");

                for (int i = 0; i < configFiles.count(); ++i) {
                    ConfigFile &cf = configFiles[i];
                    if (cf.filePath != configChecksums.at(i).first)
                        throw Exception("the cached config file names do not match the current set (or their order changed)");
                    cf.checksum = configChecksums.at(i).second;
                }
                useCache = true;

#if defined(AM_TIME_CONFIG_PARSING)
                qCDebug(LogSystem) << "Config parsing: cache loaded after" << (timer.nsecsElapsed() / 1000) << "usec";
#endif
            } catch (const Exception &e) {
                if (deploymentWarnings)
                    *deploymentWarnings << qL1S("Failed to read config cache:") + qL1S(e.what());
            }
        }
    }

    // reads a single config file and calculates its hash - defined as lambda to be usable
    // both via QtConcurrent and via std:for_each
    auto readConfigFile = [&useCache, &deploymentWarnings](ConfigFile &cf) {
        QFile file(cf.filePath);
        if (!file.open(QIODevice::ReadOnly))
            throw Exception("Failed to open config file '%1' for reading.\n").arg(file.fileName());

        if (file.size() > 1024*1024)
            throw Exception("Config file '%1' is too big (> 1MB).\n").arg(file.fileName());

        cf.content = file.readAll();

        QByteArray checksum = QCryptographicHash::hash(cf.content, QCryptographicHash::Sha1);
        if (useCache && (checksum != cf.checksum)) {
            if (deploymentWarnings)
                *deploymentWarnings << qL1S("Failed to read config cache: cached config file checksums do not match current set");
            useCache = false;
        }
        cf.checksum = checksum;
    };

    try {
        if (configFiles.size() > AM_PARALLEL_THRESHOLD)
            QtConcurrent::blockingMap(configFiles, readConfigFile);
        else
            std::for_each(configFiles.begin(), configFiles.end(), readConfigFile);
    } catch (const Exception &e) {
        showParserMessage(e.errorString(), ErrorMessage);
        exit(1);
    }

#if defined(AM_TIME_CONFIG_PARSING)
    qCDebug(LogSystem) << "Config parsing" << configFiles.size() << "files: loading finished after"
                       << (timer.nsecsElapsed() / 1000) << "usec";
#endif

    if (useCache) {
        m_config = cache;
    } else if (!configFilePaths.isEmpty()) {
        auto parseConfigFile = [](ConfigFile &cf) {
            // we want to replace ${CONFIG_PWD} (when at the start of a value) with the abs. path
            // to the config file it appears in, similar to qmake's $$_PRO_FILE_PWD
            QString configFilePath = QFileInfo(cf.filePath).absolutePath();

            auto replaceConfigPwd = [configFilePath](const QVariant &value) -> QVariant {
                if (value.type() == QVariant::String) {
                    QString str = value.toString();
                    if (str.startsWith(qSL("${CONFIG_PWD}")))
                        return QVariant(configFilePath + str.midRef(13));
                }
                return value;
            };

            QtYaml::ParseError parseError;
            QVector<QVariant> docs = QtYaml::variantDocumentsFromYamlFiltered(cf.content, replaceConfigPwd, &parseError);

            if (parseError.error != QJsonParseError::NoError) {
                throw Exception("Could not parse config file '%1', line %2, column %3: %4.\n")
                        .arg(cf.filePath).arg(parseError.line).arg(parseError.column)
                        .arg(parseError.errorString());
            }

            try {
                checkYamlFormat(docs, 2 /*number of expected docs*/, { "am-configuration" }, 1);
            } catch (const Exception &e) {
                throw Exception("Could not parse config file '%1': %2.\n")
                        .arg(cf.filePath).arg(e.errorString());
            }
            cf.config = docs.at(1).toMap();
        };

        try {
            if (configFiles.size() > AM_PARALLEL_THRESHOLD)
                QtConcurrent::blockingMap(configFiles, parseConfigFile);
            else
                std::for_each(configFiles.begin(), configFiles.end(), parseConfigFile);
        } catch (const Exception &e) {
            showParserMessage(e.errorString(), ErrorMessage);
            exit(1);
        }

        // we cannot parallelize this step, since subsequent config files can overwrite
        // or append to values
        m_config = configFiles.at(0).config;
        for (int i = 1; i < configFiles.size(); ++i)
            mergeConfig(configFiles.at(i).config);

        if (!noConfigCache) {
            try {
                QFile cacheFile(cacheFilePath);
                if (!cacheFile.open(QFile::WriteOnly | QFile::Truncate))
                    throw Exception(cacheFile, "failed to open file for writing");

                QDataStream ds(&cacheFile);
                QVector<QPair<QString, QByteArray>> configChecksums;
                for (const ConfigFile &cf : qAsConst(configFiles))
                    configChecksums.append(qMakePair(cf.filePath, cf.checksum));

                ds << configChecksums << m_config;

                if (ds.status() != QDataStream::Ok)
                    throw Exception("error writing config cache content");
            } catch (const Exception &e) {
                if (deploymentWarnings)
                    *deploymentWarnings << qL1S("Failed to write config cache: ") + qL1S(e.what());
            }
        }
#if defined(AM_TIME_CONFIG_PARSING)
        qCDebug(LogSystem) << "Config parsing" << configFiles.size() << "files: parsing finished after"
                           << (timer.nsecsElapsed() / 1000) << "usec";
#endif
    }

    const QStringList options = m_clp.values(qSL("o"));
    for (const QString &option : options) {
        QtYaml::ParseError parseError;
        QVector<QVariant> docs = QtYaml::variantDocumentsFromYaml(option.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            showParserMessage(QString::fromLatin1("Could not parse --option value, column %1: %2.\n")
                              .arg(parseError.column).arg(parseError.errorString()),
                              ErrorMessage);
            exit(1);
        }
        if (docs.size() != 1) {
            showParserMessage(QString::fromLatin1("Could not parse --option value: Invalid document format.\n"),
                              ErrorMessage);
            exit(1);
        }
        mergeConfig(docs.at(0).toMap());
    }

#if defined(AM_TIME_CONFIG_PARSING)
    qCDebug(LogSystem) << "Config parsing" << options.size() << "-o options: parsing finished after"
                       << (timer.nsecsElapsed() / 1000) << "usec";
#endif

    // QML cannot cope with invalid QVariants and QDataStream cannot cope with nullptr inside a
    // QVariant ... the workaround is to save invalid variants to the cache and fix them up
    // afterwards:
    fixNullValuesForQml(m_config);
}

QT_END_NAMESPACE_AM
