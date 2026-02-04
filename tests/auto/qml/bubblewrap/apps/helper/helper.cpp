// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtCore/QtEnvironmentVariables>
#include <QtCore/QFile>
#include "helper.h"

Helper::Helper(QObject *parent)
    : QObject{parent}
{}

QByteArray Helper::getEnv(const QByteArray &envName)
{
    return qgetenv(envName.constData());
}

bool Helper::pathExists(const QString &path)
{
    return QFile::exists(path);
}

QString Helper::readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return { };
    return QString::fromUtf8(f.readAll());
}

bool Helper::canWrite(const QString &dirPath)
{
    const QString testFile = dirPath + u"/qt-am-bwrap-write-test";
    QFile f(testFile);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.close();
    f.remove();
    return true;
}
