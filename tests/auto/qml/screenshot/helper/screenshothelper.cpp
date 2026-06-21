// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtCore/QFile>
#include <QtGui/QImage>
#include "screenshothelper.h"

ScreenshotHelper::ScreenshotHelper(QObject *parent)
    : QObject(parent)
{}

QString ScreenshotHelper::makeTempDir()
{
    m_tempDir = std::make_unique<QTemporaryDir>();
    return m_tempDir->isValid() ? m_tempDir->path() : QString();
}

bool ScreenshotHelper::fileExists(const QString &path) const
{
    return QFile::exists(path);
}

QSize ScreenshotHelper::sizeOf(const QString &path) const
{
    return QImage(path).size();
}

QString ScreenshotHelper::colorAt(const QString &path, int x, int y) const
{
    const QImage img(path);
    if (img.isNull() || !img.rect().contains(x, y))
        return { };
    return img.pixelColor(x, y).name(); // "#rrggbb"
}
