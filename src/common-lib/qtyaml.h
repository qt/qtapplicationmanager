/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: http://www.qt.io/ or http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL3-PELAGICORE$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#pragma once

#include <QJsonParseError>
#include <QVector>
#include <QByteArray>
#include <QString>
#include <QVariant>


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

QVector<QVariant> variantDocumentsFromYaml(const QByteArray &yaml, ParseError *error = 0);

enum YamlStyle { FlowStyle, BlockStyle };

QByteArray yamlFromVariantDocuments(const QVector<QVariant> &maps, YamlStyle style = BlockStyle);

} // namespace QtYaml
