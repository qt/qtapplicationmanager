/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
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

#include <QVariant>
#include <QRegExp>
#include <QDebug>
#include <QtNumeric>

#include <yaml.h>

#include "global.h"
#include "qtyaml.h"


namespace QtYaml {

static QVariant convertYamlNodeToVariant(yaml_document_t *doc, yaml_node_t *node = 0)
{
    QVariant result;

    if (!doc)
        return result;
    if (!node)
        node = yaml_document_get_root_node(doc);
    if (!node)
        return result;

    switch (node->type) {
    case YAML_SCALAR_NODE: {
        QByteArray ba(reinterpret_cast<const char *>(node->data.scalar.value), int(node->data.scalar.length));

        if (node->data.scalar.style == YAML_SINGLE_QUOTED_SCALAR_STYLE
                || node->data.scalar.style == YAML_DOUBLE_QUOTED_SCALAR_STYLE) {
            result = QString::fromUtf8(ba);
            break;
        }
        enum ValueIndex {
            ValueNull,
            ValueTrue,
            ValueFalse,
            ValueNaN,
            ValueInf
        };

        struct StaticMapping
        {
            const char *text;
            ValueIndex index;
        };

        static QVariant staticValues[] = {
            QVariant(),        // ValueNull
            QVariant(true),    // ValueTrue
            QVariant(false),   // ValueFalse
            QVariant(qQNaN()), // ValueNaN
            QVariant(qInf()),  // ValueInf
        };

        static StaticMapping staticMappings[] = { // keep this sorted for bsearch !!
            { "",      ValueNull },
            { ".INF",  ValueInf },
            { ".Inf",  ValueInf },
            { ".NAN",  ValueNaN },
            { ".NaN",  ValueNaN },
            { ".inf",  ValueInf },
            { ".nan",  ValueNaN },
            { "FALSE", ValueFalse },
            { "False", ValueFalse },
            { "N",     ValueFalse },
            { "NO",    ValueFalse },
            { "NULL",  ValueNull },
            { "No",    ValueFalse },
            { "Null",  ValueNull },
            { "OFF",   ValueFalse },
            { "Off",   ValueFalse },
            { "ON",    ValueTrue },
            { "On",    ValueTrue },
            { "TRUE",  ValueTrue },
            { "True",  ValueTrue },
            { "Y",     ValueTrue },
            { "YES",   ValueTrue },
            { "Yes",   ValueTrue },
            { "false", ValueFalse },
            { "n",     ValueFalse },
            { "no",    ValueFalse },
            { "null",  ValueNull },
            { "off",   ValueFalse },
            { "on",    ValueTrue },
            { "true",  ValueTrue },
            { "y",     ValueTrue },
            { "yes",   ValueTrue },
            { "~",     ValueNull }
        };

        StaticMapping key { ba.constData(), ValueNull };
        auto found = bsearch(&key,
                             staticMappings,
                             sizeof(staticMappings)/sizeof(staticMappings[0]),
                             sizeof(staticMappings[0]),
                             [](const void *m1, const void *m2) {
                                 return strcmp(static_cast<const StaticMapping *>(m1)->text,
                                               static_cast<const StaticMapping *>(m2)->text); });

        if (found) {
            result = staticValues[static_cast<StaticMapping *>(found)->index];
            break;
        }

        static QRegExp numberRegExps[] = {
            QRegExp("[-+]?0b[0-1_]+"),        // binary
            QRegExp("[-+]?0x[0-9a-fA-F_]+"),  // hexadecimal
            QRegExp("[-+]?0[0-7_]+"),         // octal
            QRegExp("[-+]?(0|[1-9][0-9_]*)"), // decimal
            QRegExp("[-+]?([0-9][0-9_]*)?\\.[0-9.]*([eE][-+][0-9]+)?"), // float
            QRegExp()
        };

        QString str = QString::fromUtf8(ba);
        for (int numberIndex = 0; !numberRegExps[numberIndex].isEmpty(); ++numberIndex) {
            if (numberRegExps[numberIndex].exactMatch(str)) {
                bool ok = false;
                QVariant val;

                if (numberIndex == 4) {
                    val = ba.replace('_', "").toDouble(&ok);
                } else {
                    int base = 10;

                    switch (numberIndex) {
                    case 0: base = 2; ba.replace("0b", ""); break;
                    case 1: base = 16; break;
                    case 2: base = 8; break;
                    case 3: base = 10; break;
                    }

                    qint64 s64 = ba.replace('_', "").toLongLong(&ok, base);
                    if (ok && (s64 <= std::numeric_limits<qint32>::max())) {
                        val = qint32(s64);
                    } else if (ok) {
                        val = s64;
                    } else {
                        quint64 u64 = ba.replace('_', "").toULongLong(&ok, base);

                        if (ok && (u64 <= std::numeric_limits<quint32>::max()))
                            val = quint32(u64);
                        else if (ok)
                            val = u64;
                    }
                }
                if (ok) {
                    result = val;
                    break;
                }
            }
        }

        if (result.isNull())
            result = QString::fromUtf8(ba);
        break;
    }
    case YAML_SEQUENCE_NODE: {
        QVariantList array;
        for (auto seq = node->data.sequence.items.start; seq < node->data.sequence.items.top; ++seq) {
            yaml_node_t *seqNode = yaml_document_get_node(doc, *seq);
            if (seqNode)
                array.append(convertYamlNodeToVariant(doc, seqNode));
            else
                array.append(QVariant());
        }
        result = array;
        break;
    }
    case YAML_MAPPING_NODE: {
        QVariantMap object;
        for (auto map = node->data.mapping.pairs.start; map < node->data.mapping.pairs.top; ++map) {
            yaml_node_t *keyNode = yaml_document_get_node(doc, map->key);
            yaml_node_t *valueNode = yaml_document_get_node(doc, map->value);
            if (keyNode && valueNode) {
                QVariant key = convertYamlNodeToVariant(doc, keyNode);
                QString keyStr = key.toString();

                if (key.type() != QVariant::String)
                    qWarning() << "YAML Parser: converting non-string mapping key to string for JSON compatibility";
                if (object.contains(keyStr))
                    qWarning() << "YAML Parser: duplicate key" << keyStr << "found in mapping";

                object.insert(keyStr, convertYamlNodeToVariant(doc, valueNode));
            }
        }
        result = object;
        break;
    }
    default:
        break;
    }
    return result;
}



QVector<QVariant> variantDocumentsFromYaml(const QByteArray &yaml, ParseError *error)
{
    QVector<QVariant> result;

    if (error)
        *error = ParseError();

    yaml_parser_t p;
    if (yaml_parser_initialize(&p)) {
        yaml_parser_set_input_string(&p, (const uchar *) yaml.constData(), yaml.size());

        forever {
            yaml_document_t doc;
            if (!yaml_parser_load(&p, &doc)) {
                if (error) {
                    switch (p.error) {
                    case YAML_READER_ERROR:
                        *error = ParseError(p.problem, -1, -1, int(p.problem_offset));
                        break;
                    case YAML_SCANNER_ERROR:
                    case YAML_PARSER_ERROR:
                        *error = ParseError(p.problem, int(p.problem_mark.line), int(p.problem_mark.column), int(p.problem_mark.index));
                        break;
                    default:
                        break;
                    }
                }
                result << QVariant();
                break;
            } else {
                yaml_node_t *root = yaml_document_get_root_node(&doc);
                if (!root)
                    break;

                result << convertYamlNodeToVariant(&doc);
            }
            yaml_document_delete(&doc);
        }
        yaml_parser_delete(&p);
    } else if (error) {
        *error = ParseError(qSL("could not initialize YAML parser"));
    }
    return result;
}

static inline void yerr(int result) throw(std::exception)
{
    if (!result)
        throw std::exception();
}

static void emitYamlScalar(yaml_emitter_t *e, const QByteArray &ba, bool quoting = false)
{
    yaml_event_t event;
    yerr(yaml_scalar_event_initialize(&event,
                                      nullptr,
                                      nullptr,
                                      reinterpret_cast<yaml_char_t *>(const_cast<char *>(ba.constData())),
                                      ba.size(),
                                      1,
                                      1,
                                      quoting ? YAML_SINGLE_QUOTED_SCALAR_STYLE : YAML_ANY_SCALAR_STYLE));
    yerr(yaml_emitter_emit(e, &event));
}

static void emitYaml(yaml_emitter_t *e, const QVariant &value, YamlStyle style)
{
    yaml_event_t event;

    switch (value.type()) {
    default:
    case QVariant::Invalid:
        emitYamlScalar(e, "~");
        break;
    case QVariant::Bool:
        emitYamlScalar(e, value.toBool() ? "true" : "false");
        break;
    case QVariant::Int:
    case QVariant::LongLong:
        emitYamlScalar(e, QByteArray::number(value.toLongLong()));
        break;
    case QVariant::UInt:
    case QVariant::ULongLong:
        emitYamlScalar(e, QByteArray::number(value.toULongLong()));
        break;
    case QVariant::Double:
        emitYamlScalar(e, QByteArray::number(value.toDouble()));
        break;
    case QVariant::String:
        emitYamlScalar(e, value.toString().toUtf8(), true);
        break;
    case QVariant::List:
    case QVariant::StringList: {
        yerr(yaml_sequence_start_event_initialize(&event, nullptr, nullptr, 1, style == FlowStyle ? YAML_FLOW_SEQUENCE_STYLE : YAML_BLOCK_SEQUENCE_STYLE));
        yerr(yaml_emitter_emit(e, &event));

        foreach (const QVariant &listValue, value.toList())
            emitYaml(e, listValue, style);

        yerr(yaml_sequence_end_event_initialize(&event));
        yerr(yaml_emitter_emit(e, &event));
        break;
    }
    case QVariant::Map: {
        yerr(yaml_mapping_start_event_initialize(&event, nullptr, nullptr, 1, style == FlowStyle ? YAML_FLOW_MAPPING_STYLE : YAML_BLOCK_MAPPING_STYLE));
        yerr(yaml_emitter_emit(e, &event));

        QVariantMap map = value.toMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            emitYamlScalar(e, it.key().toUtf8());
            emitYaml(e, it.value(), style);
        }

        yerr(yaml_mapping_end_event_initialize(&event));
        yerr(yaml_emitter_emit(e, &event));
        break;
    }
    }
}


QByteArray yamlFromVariantDocuments(const QVector<QVariant> &documents, YamlStyle style)
{
    QByteArray out;

    yaml_emitter_t e;
    yaml_emitter_initialize(&e);
    yaml_emitter_set_output(&e, [] (void *data, unsigned char *buffer, size_t size) { static_cast<QByteArray *>(data)->append(reinterpret_cast<char *>(buffer), int(size)); return 1; }, &out);

    //if (style == FlowStyle)
    yaml_emitter_set_width(&e, 80);
    yaml_emitter_set_indent(&e, 2);

    try {
        yaml_event_t event;
        yerr(yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING));
        yerr(yaml_emitter_emit(&e, &event));

        bool first = true;
        yaml_version_directive_t yamlVersion { 1, 1 };

        foreach (const QVariant &doc, documents) {
            yerr(yaml_document_start_event_initialize(&event, first ? &yamlVersion : nullptr, nullptr, nullptr, 0));
            yerr(yaml_emitter_emit(&e, &event));

            first = false;

            emitYaml(&e, doc, style);

            yerr(yaml_document_end_event_initialize(&event, 1));
            yerr(yaml_emitter_emit(&e, &event));
        }

        yerr(yaml_stream_end_event_initialize(&event));
        yerr(yaml_emitter_emit(&e, &event));
    } catch (const std::exception &) {
        out.clear();
    }
    yaml_emitter_delete(&e);

    return out;
}

}
