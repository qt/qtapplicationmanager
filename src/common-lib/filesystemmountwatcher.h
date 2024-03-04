// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef FILESYSTEMMOUNTWATCHER_H
#define FILESYSTEMMOUNTWATCHER_H

#include <QtCore/QObject>
#include <QtCore/QMultiMap>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class FileSystemMountWatcherPrivate;

class FileSystemMountWatcher : public QObject
{
    Q_OBJECT

public:
    FileSystemMountWatcher(QObject *parent = nullptr);
    ~FileSystemMountWatcher() override;

    static QMultiMap<QString, QString> currentMountPoints();

    void addMountPoint(const QString &directory);
    void removeMountPoint(const QString &directory);

    // Auto-test API, only functional on Linux. Needs to be called before constructor:
    static bool setMountTabFileForTesting(const QString &mtabFile);

Q_SIGNALS:
    void mountChanged(const QString &mountPoint, const QString &device);

private:
    static FileSystemMountWatcherPrivate *d;
};

QT_END_NAMESPACE_AM

#endif // FILESYSTEMMOUNTWATCHER_H
