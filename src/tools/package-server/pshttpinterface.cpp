// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <cstdio>

#include <QHttpServer>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QBuffer>
#include <QtAppManCommon/exception.h>

#include "psconfiguration.h"
#include "pspackages.h"
#include "pshttpinterface.h"
#include "pshttpinterface_p.h"
#include "package-server.h"


QT_USE_NAMESPACE_AM
using namespace Qt::StringLiterals;


static QJsonObject asJson(const QMap<QString, QString> &map)
{
    QJsonObject result;
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        result.insert(it.key(), it.value());
    return result;
}


PSHttpInterface::PSHttpInterface(PSConfiguration *cfg, QObject *parent)
    : QObject(parent)
    , d(new PSHttpInterfacePrivate)
{
    d->cfg = cfg;
    d->server = new QHttpServer(this);
}

void PSHttpInterface::listen()
{
    const auto hostStr = d->cfg->listenUrl.host();
    QHostAddress host;
    if (hostStr == u"any")
        host = QHostAddress(QHostAddress::Any);
    else if (hostStr == u"localhost")
        host = QHostAddress(QHostAddress::LocalHost);
    else
        host = QHostAddress(hostStr);

    auto port = quint16(d->cfg->listenUrl.port(0));

    auto usedPort = d->server->listen(host, port);
    if (usedPort == 0)
        throw Exception("failed to listen on %1:%2").arg(host.toString()).arg(port);
    d->listenAddress = host.toString() + u':' + QString::number(usedPort);
}

QString PSHttpInterface::listenAddress() const
{
    return d->listenAddress;
}

void PSHttpInterface::setupRouting(PSPackages *packages)
{
    static constexpr auto GetOrPost = QHttpServerRequest::Method::Post | QHttpServerRequest::Method::Get;

    d->server->route(u"/hello"_s, GetOrPost, [this](const QHttpServerRequest &req) {
        const auto query = req.query();
        QString status = u"ok"_s;

        if (query.queryItemValue(u"project-id"_s) != d->cfg->projectId)
            status = u"incompatible-project-id"_s;

        return QJsonObject { { u"status"_s, status } };
    });

    d->server->route(u"/package/list"_s, GetOrPost, [packages](const QHttpServerRequest &req) {
        const auto query = req.query();
        const QString architecture = query.queryItemValue(u"architecture"_s);
        const QString category = query.queryItemValue(u"category"_s);
        const QString filter = query.queryItemValue(u"filter"_s);

        QJsonArray pkgs;

        const auto spList = packages->byArchitecture(architecture);

        for (const PSPackage *sp : spList) {
            if (!category.isEmpty() && sp->packageInfo->categories().contains(category))
                continue;
            if (!filter.isEmpty()) {
                bool match = false;
                const auto names = sp->packageInfo->names();
                for (const auto &name : names) {
                    if (name.contains(filter)) {
                        match = true;
                        break;
                    }
                }
                if (match)
                    continue;
            }

            pkgs.append(QJsonObject {
                { u"id"_s, sp->packageInfo->id() },
                { u"architecture"_s, sp->architecture },
                { u"names"_s, asJson(sp->packageInfo->names()) },
                { u"descriptions"_s, asJson(sp->packageInfo->descriptions()) },
                { u"version"_s, sp->packageInfo->version() },
                { u"categories"_s, QJsonArray::fromStringList(sp->packageInfo->categories()) },
                { u"iconUrl"_s, QString { u"package/icon?id="_s + sp->packageInfo->id() } },
                });
        }

        return QHttpServerResponse { pkgs };
    });

    d->server->route(u"/package/icon"_s, GetOrPost, [packages](const QHttpServerRequest &req) {
        const auto query = req.query();
        QString id = query.queryItemValue(u"id"_s);
        const QString architecture = query.queryItemValue(u"architecture"_s);

        if (auto *sp = packages->byIdAndArchitecture(id, architecture)) {
            if (!sp->iconData.isEmpty())
                return QHttpServerResponse("image/png", sp->iconData);
        }
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
    });

    d->server->route(u"/package/download"_s, GetOrPost, [this, packages](const QHttpServerRequest &req) {
        const auto query = req.query();
        QString id = query.queryItemValue(u"id"_s);
        const QString architecture = query.queryItemValue(u"architecture"_s);
        const QString hardwareId = query.queryItemValue(u"hardware-id"_s);

        if (auto *sp = packages->byIdAndArchitecture(id, architecture)) {
            if (!d->cfg->storeSignCertificate.isEmpty()) {
                try {
                    QBuffer buffer;
                    buffer.open(QIODevice::WriteOnly);
                    packages->storeSign(sp, hardwareId, &buffer);

                    return QHttpServerResponse("application/octet-stream", buffer.data());
                } catch (const Exception &e) {
                    colorOut() << ColorPrint::red << " x failed" << ColorPrint::reset << ": "
                               << e.errorString();
                    return QHttpServerResponse { QHttpServerResponse::StatusCode::InternalServerError };
                }

            } else {
                QFile f(sp->filePath);
                if (f.open(QIODevice::ReadOnly))
                    return QHttpServerResponse("application/octet-stream", f.readAll());
            }
        }

        return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
    });

    d->server->route(u"/package/upload"_s, QHttpServerRequest::Method::Put, [packages](const QHttpServerRequest &req) {
        try {
            const QByteArray pkgData = req.body();
            if (!pkgData.startsWith("\x1f\x8b")) // .gz
                throw Exception("wrong package format");

            QTemporaryFile f;
            if (!f.open())
                throw Exception("coud not open temporary file");

            if (f.write(pkgData) != pkgData.size())
                throw Exception("coud not write to temporary file");

            f.setAutoRemove(false);
            QString pkgFileName = f.fileName();
            f.close();

            colorOut() << "> Upload via HTTP:";

            auto [result, sp] = packages->upload(pkgFileName);

            QString resultStr;
            switch (result) {
            case PSPackages::UploadResult::Added    : resultStr = u"added"_s; break;
            case PSPackages::UploadResult::Updated  : resultStr = u"updated"_s; break;
            case PSPackages::UploadResult::NoChanges: resultStr = u"no changes"_s; break;
            }

            return QHttpServerResponse { QJsonObject {
                { u"status"_s, u"ok"_s },
                { u"result"_s, resultStr },
                { u"id"_s, sp->id },
                { u"architecture"_s, sp->architectureOrAll() }
            } };
        } catch (const Exception &e) {
            return QHttpServerResponse { QJsonObject {
                { u"status"_s, u"fail"_s },
                { u"message"_s, e.errorString() }
            } };
        }
    });

    d->server->route(u"/package/remove"_s, QHttpServerRequest::Method::Post, [packages](const QHttpServerRequest &req) {
        const auto query = req.query();
        const QString id = query.queryItemValue(u"id"_s);
        const QString architecture = query.queryItemValue(u"architecture"_s);

        colorOut() << "> Remove via HTTP:";

        int removeCount = packages->removeIf([id, architecture](PSPackage *sp) {
            return (sp->id == id) && (architecture.isEmpty() || (architecture == sp->architectureOrAll()));
        });

        if (!removeCount) {
            colorOut() << ColorPrint::blue << " = skipping " << ColorPrint::bcyan << id << ColorPrint::reset
                       << " [" << (architecture.isEmpty() ? u"<any>"_s : architecture) << "] (no match)";
        }
        return QJsonObject { { u"status"_s, removeCount ? u"ok"_s : u"fail"_s },
                             { u"removed"_s,  removeCount } };
    });

    d->server->route(u"/category/list"_s, GetOrPost, [packages](const QHttpServerRequest &) {
        return QJsonArray::fromStringList(packages->categories());
    });
}

#include "moc_pshttpinterface.cpp"
