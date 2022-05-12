/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
**
**
**
**
**
**
**
**
******************************************************************************/

#pragma once

#include <QByteArray>
#include <QFile>
#include <QtAppManCommon/global.h>

QT_BEGIN_NAMESPACE_AM

class SysFsReader
{
public:
    SysFsReader(const QByteArray &path, int maxRead = 2048);
    ~SysFsReader();
    bool isOpen() const;
    QByteArray fileName() const;
    QByteArray readValue() const;

private:
    mutable QFile m_fd; // seek() is non-const
    QByteArray m_path;
    mutable QByteArray m_buffer;

    Q_DISABLE_COPY(SysFsReader)
};

QT_END_NAMESPACE_AM
