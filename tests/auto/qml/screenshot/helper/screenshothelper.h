// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef SCREENSHOTHELPER_H
#define SCREENSHOTHELPER_H

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QSize>
#include <QtCore/QTemporaryDir>
#include <QtQml/QQmlEngine>

class ScreenshotHelper : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit ScreenshotHelper(QObject *parent = nullptr);

    // creates and returns a unique, empty directory for the test's screenshots
    Q_INVOKABLE QString makeTempDir();
    // whether a file exists at the given path
    Q_INVOKABLE bool fileExists(const QString &path) const;
    // the on-disk pixel size of the image at the given file path (invalid QSize if not loadable)
    Q_INVOKABLE QSize sizeOf(const QString &path) const;
    // the color at (x, y) as a "#rrggbb" string (empty if the image cannot be loaded)
    Q_INVOKABLE QString colorAt(const QString &path, int x, int y) const;

private:
    std::unique_ptr<QTemporaryDir> m_tempDir;
};

#endif // SCREENSHOTHELPER_H
