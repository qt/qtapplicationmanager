/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#pragma once
#include <qglobal.h>

#if defined(AM_BUILD_APPMAN)
#  define AM_EXPORT Q_DECL_EXPORT
#else
#  define AM_EXPORT Q_DECL_IMPORT
#endif

#include <QLoggingCategory>

AM_EXPORT Q_DECLARE_LOGGING_CATEGORY(LogSystem)
AM_EXPORT Q_DECLARE_LOGGING_CATEGORY(LogInstaller)
AM_EXPORT Q_DECLARE_LOGGING_CATEGORY(LogWayland)
AM_EXPORT Q_DECLARE_LOGGING_CATEGORY(LogQml)
AM_EXPORT Q_DECLARE_LOGGING_CATEGORY(LogNotifications)
AM_EXPORT Q_DECLARE_LOGGING_CATEGORY(LogQmlRuntime)
AM_EXPORT Q_DECLARE_LOGGING_CATEGORY(LogQmlIpc)

void colorLogToStderr(QtMsgType msgType, const QMessageLogContext &context, const QString &message);

QString hardwareId();

void am_trace(QDebug);
template <typename T, typename... TRest> void am_trace(QDebug dbg, T t, TRest... trest)
{ dbg << t; am_trace(dbg, trest...); }

#define AM_TRACE(category, ...) \
    for (bool qt_category_enabled = category().isDebugEnabled(); qt_category_enabled; qt_category_enabled = false) { \
        am_trace(QMessageLogger(__FILE__, __LINE__, __FUNCTION__, category().categoryName()).debug(), "TRACE", __FUNCTION__, __VA_ARGS__); \
    }

// make the source a lot less ugly and more readable (until we can finally use user defined literals)
#define qL1S(x) QLatin1String(x)
#define qL1C(x) QLatin1Char(x)
#define qSL(x) QStringLiteral(x)
