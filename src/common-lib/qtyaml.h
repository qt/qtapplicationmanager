/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#pragma once

#include <functional>

#include <QJsonParseError>
#include <QVector>
#include <QByteArray>
#include <QString>
#include <QVariant>

QT_BEGIN_NAMESPACE

namespace QtYaml {

class ParseError
{
public:
    ParseError() { }

    QString errorString() const { return m_errorString; }

    int line = -1;
    int column = -1;

    // QJsonParseError compatibility
    int offset = -1;
    QJsonParseError::ParseError error = QJsonParseError::NoError;

    // not really public
    ParseError(const QString &_errorString, int _line = -1, int _column = -1, int _offset = -1)
        : line(_line)
        , column(_column)
        , offset(_offset)
        , error(QJsonParseError::UnterminatedObject) // not really, but there are no suitable options
        , m_errorString(_errorString)
    { }

private:
    QString m_errorString;
};

QVector<QVariant> variantDocumentsFromYaml(const QByteArray &yaml, ParseError *error = nullptr);

enum YamlStyle { FlowStyle, BlockStyle };

QByteArray yamlFromVariantDocuments(const QVector<QVariant> &maps, YamlStyle style = BlockStyle);

} // namespace QtYaml

QT_END_NAMESPACE
