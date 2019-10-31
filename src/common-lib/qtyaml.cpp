/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#include <QVariant>
#include <QRegularExpression>
#include <QDebug>
#include <QtNumeric>
#include <QTextCodec>

#include <yaml.h>

#include "global.h"
#include "qtyaml.h"
#include "exception.h"

QT_BEGIN_NAMESPACE_AM


namespace QtYaml {

static QVariant convertYamlNodeToVariant(yaml_document_t *doc, yaml_node_t *node)
{
    QVariant result;

    if (!doc)
        return result;
    if (!node)
        return result;

    switch (node->type) {
    case YAML_SCALAR_NODE: {
        const QByteArray ba = QByteArray::fromRawData(reinterpret_cast<const char *>(node->data.scalar.value),
                                                int(node->data.scalar.length));

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
            QVariant(),                    // ValueNull
            QVariant(true),                // ValueTrue
            QVariant(false),               // ValueFalse
            QVariant(qQNaN()),             // ValueNaN
            QVariant(qInf()),              // ValueInf
        };

        static const StaticMapping staticMappings[] = { // keep this sorted for bsearch !!
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

        static const char *firstCharStaticMappings = ".FNOTYfnoty~";
        char firstChar = ba.isEmpty() ? 0 : ba.at(0);

        if (strchr(firstCharStaticMappings, firstChar)) { // cheap check to avoid expensive bsearch
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
        }

        QString str = QString::fromUtf8(ba);
        result = str;

        if ((firstChar >= '0' && firstChar <= '9')   // cheap check to avoid expensive regexps
                || firstChar == '+' || firstChar == '-' || firstChar == '.') {
            static const QRegExp numberRegExps[] = {
                QRegExp(qSL("[-+]?0b[0-1_]+")),        // binary
                QRegExp(qSL("[-+]?0x[0-9a-fA-F_]+")),  // hexadecimal
                QRegExp(qSL("[-+]?0[0-7_]+")),         // octal
                QRegExp(qSL("[-+]?(0|[1-9][0-9_]*)")), // decimal
                QRegExp(qSL("[-+]?([0-9][0-9_]*)?\\.[0-9.]*([eE][-+][0-9]+)?")), // float
                QRegExp()
            };

            for (int numberIndex = 0; !numberRegExps[numberIndex].isEmpty(); ++numberIndex) {
                if (numberRegExps[numberIndex].exactMatch(str)) {
                    bool ok = false;
                    QVariant val;

                    // YAML allows _ as a grouping separator
                    if (str.contains(qL1C('_')))
                        str = str.replace(qL1C('_'), qSL(""));

                    if (numberIndex == 4) {
                        val = str.toDouble(&ok);
                    } else {
                        int base = 10;

                        switch (numberIndex) {
                        case 0: base = 2; str.replace(qSL("0b"), qSL("")); break; // Qt chokes on 0b
                        case 1: base = 16; break;
                        case 2: base = 8; break;
                        case 3: base = 10; break;
                        }

                        qint64 s64 = str.toLongLong(&ok, base);
                        if (ok && (s64 <= std::numeric_limits<qint32>::max())) {
                            val = qint32(s64);
                        } else if (ok) {
                            val = s64;
                        } else {
                            quint64 u64 = str.toULongLong(&ok, base);

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
        }
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


static inline void yerr(int result) Q_DECL_NOEXCEPT_EXPR(false)
{
    if (!result)
        throw std::exception();
}

static void emitYamlScalar(yaml_emitter_t *e, const QByteArray &ba, bool quoting = false) Q_DECL_NOEXCEPT_EXPR(false)
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

static void emitYaml(yaml_emitter_t *e, const QVariant &value, YamlStyle style) Q_DECL_NOEXCEPT_EXPR(false)
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

        const QVariantList list = value.toList();
        for (const QVariant &listValue : list)
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

        for (const QVariant &doc : documents) {
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


} // namespace QtYaml


class YamlParserPrivate
{
public:
    QByteArray data;
    QTextCodec *codec = nullptr;
    bool parsedHeader = false;
    yaml_parser_t parser;
    yaml_event_t event;

};


YamlParser::YamlParser(const QByteArray &data)
    : d(new YamlParserPrivate)
{
    d->data = data;

    memset(&d->parser, 0, sizeof(d->parser));
    memset(&d->event, 0, sizeof(d->event));

    if (!yaml_parser_initialize(&d->parser))
        throw Exception("Failed to initialize YAML parser");
    yaml_parser_set_input_string(&d->parser, reinterpret_cast<const unsigned char *>(d->data.constData()),
                                 static_cast<size_t>(d->data.size()));
    nextEvent();
    if (d->event.type != YAML_STREAM_START_EVENT)
        throw Exception("Invalid YAML data");
    switch (d->event.data.stream_start.encoding) {
    default:
    case YAML_UTF8_ENCODING:    d->codec = QTextCodec::codecForName("UTF-8"); break;
    case YAML_UTF16LE_ENCODING: d->codec = QTextCodec::codecForName("UTF-16LE"); break;
    case YAML_UTF16BE_ENCODING: d->codec = QTextCodec::codecForName("UTF-16BE"); break;
    }
}

YamlParser::~YamlParser()
{
    if (d->event.type != YAML_NO_EVENT)
        yaml_event_delete(&d->event);
    yaml_parser_delete(&d->parser);
    delete d;
}

QVector<QVariant> YamlParser::parseAllDocuments(const QByteArray &yaml)
{
    YamlParser p(yaml);
    QVector<QVariant> result;
    while (p.nextDocument())
        result << p.parseMap();
    return result;
}

QPair<QString, int> YamlParser::parseHeader()
{
    if (d->parsedHeader)
        throw Exception("Already parsed header");
    if (d->event.type != YAML_STREAM_START_EVENT)
        throw Exception("Header must be the first document of a file");
    if (!nextDocument())
        throw Exception("No header document available");
    if (!isMap())
        throw Exception("Header document is not a map");

    QPair<QString, int> result;

    Fields fields = {
        { "formatType", true, YamlParser::Scalar, [&result](YamlParser *parser) {
              result.first = parser->parseScalar().toString(); } },
        { "formatVersion", true, YamlParser::Scalar, [&result](YamlParser *parser) {
              result.second = parser->parseScalar().toInt(); } }
    };

    parseFields(fields);

    d->parsedHeader = true;
    return result;
}

bool YamlParser::nextDocument()
{
    while (d->event.type != YAML_STREAM_END_EVENT) {
        nextEvent();
        if (d->event.type == YAML_DOCUMENT_START_EVENT) {
            nextEvent(); // advance to the first child or end-of-document
            return true;
        }
    }
    return false;
}

void YamlParser::nextEvent()
{
    if (d->event.type != YAML_NO_EVENT)
        yaml_event_delete(&d->event);

    do {
        if (!yaml_parser_parse(&d->parser, &d->event))
            throw YamlParserException(this, "cannot get next event");
    } while (d->event.type == YAML_NO_EVENT);
}

bool YamlParser::isScalar() const
{
    return d->event.type == YAML_SCALAR_EVENT;
}

QString YamlParser::parseString() const
{
    if (!isScalar())
        return QString{};

    Q_ASSERT(d->event.data.scalar.value);
    Q_ASSERT(static_cast<int>(d->event.data.scalar.length) >= 0);

    return QTextDecoder(d->codec).toUnicode(reinterpret_cast<const char *>(d->event.data.scalar.value),
                                            static_cast<int>(d->event.data.scalar.length));
}

QVariant YamlParser::parseScalar() const
{
    QString scalar = parseString();

    if (scalar.isEmpty()
            || d->event.data.scalar.style == YAML_SINGLE_QUOTED_SCALAR_STYLE
            || d->event.data.scalar.style == YAML_DOUBLE_QUOTED_SCALAR_STYLE) {
        return scalar;
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
        QString text;
        ValueIndex index;
    };

    static const QVariant staticValues[] = {
        QVariant(),                    // ValueNull
        QVariant(true),                // ValueTrue
        QVariant(false),               // ValueFalse
        QVariant(qQNaN()),             // ValueNaN
        QVariant(qInf()),              // ValueInf
    };

    static const StaticMapping staticMappings[] = { // keep this sorted for bsearch !!
        { qSL(""),      ValueNull },
        { qSL(".INF"),  ValueInf },
        { qSL(".Inf"),  ValueInf },
        { qSL(".NAN"),  ValueNaN },
        { qSL(".NaN"),  ValueNaN },
        { qSL(".inf"),  ValueInf },
        { qSL(".nan"),  ValueNaN },
        { qSL("FALSE"), ValueFalse },
        { qSL("False"), ValueFalse },
        { qSL("N"),     ValueFalse },
        { qSL("NO"),    ValueFalse },
        { qSL("NULL"),  ValueNull },
        { qSL("No"),    ValueFalse },
        { qSL("Null"),  ValueNull },
        { qSL("OFF"),   ValueFalse },
        { qSL("Off"),   ValueFalse },
        { qSL("ON"),    ValueTrue },
        { qSL("On"),    ValueTrue },
        { qSL("TRUE"),  ValueTrue },
        { qSL("True"),  ValueTrue },
        { qSL("Y"),     ValueTrue },
        { qSL("YES"),   ValueTrue },
        { qSL("Yes"),   ValueTrue },
        { qSL("false"), ValueFalse },
        { qSL("n"),     ValueFalse },
        { qSL("no"),    ValueFalse },
        { qSL("null"),  ValueNull },
        { qSL("off"),   ValueFalse },
        { qSL("on"),    ValueTrue },
        { qSL("true"),  ValueTrue },
        { qSL("y"),     ValueTrue },
        { qSL("yes"),   ValueTrue },
        { qSL("~"),     ValueNull }
    };

    static const char *firstCharStaticMappings = ".FNOTYfnoty~";
    char firstChar = scalar.isEmpty() ? 0 : scalar.at(0).toLatin1();

    if (strchr(firstCharStaticMappings, firstChar)) { // cheap check to avoid expensive bsearch
        StaticMapping key { scalar, ValueNull };
        auto found = bsearch(&key,
                             staticMappings,
                             sizeof(staticMappings) / sizeof(staticMappings[0]),
                sizeof(staticMappings[0]),
                [](const void *m1, const void *m2) {
            return static_cast<const StaticMapping *>(m1)->text.compare(static_cast<const StaticMapping *>(m2)->text);
        });

        if (found)
            return staticValues[static_cast<StaticMapping *>(found)->index];
    }

    if ((firstChar >= '0' && firstChar <= '9')   // cheap check to avoid expensive regexps
            || firstChar == '+' || firstChar == '-' || firstChar == '.') {
        static const QRegularExpression numberRegExps[] = {
            QRegularExpression(qSL("\\A[-+]?(0|[1-9][0-9_]*)\\z")), // decimal
            QRegularExpression(qSL("\\A[-+]?([0-9][0-9_]*)?\\.[0-9.]*([eE][-+][0-9]+)?\\z")), // float
            QRegularExpression(qSL("\\A[-+]?0x[0-9a-fA-F_]+\\z")),  // hexadecimal
            QRegularExpression(qSL("\\A[-+]?0b[0-1_]+\\z")),        // binary
            QRegularExpression(qSL("\\A[-+]?0[0-7_]+\\z")),         // octal
        };
        for (size_t numberIndex = 0; numberIndex < (sizeof(numberRegExps) / sizeof(*numberRegExps)); ++numberIndex) {
            if (numberRegExps[numberIndex].match(scalar).hasMatch()) {
                bool ok = false;
                QVariant val;

                // YAML allows _ as a grouping separator
                if (scalar.contains(qL1C('_')))
                    scalar = scalar.replace(qL1C('_'), qSL(""));

                if (numberIndex == 1) {
                    val = scalar.toDouble(&ok);
                } else {
                    int base = 10;

                    switch (numberIndex) {
                    case 0: base = 10; break;
                    case 2: base = 16; break;
                    case 3: base = 2; scalar.replace(qSL("0b"), qSL("")); break; // Qt chokes on 0b
                    case 4: base = 8; break;
                    }

                    qint64 s64 = scalar.toLongLong(&ok, base);
                    if (ok && (s64 <= std::numeric_limits<qint32>::max())) {
                        val = qint32(s64);
                    } else if (ok) {
                        val = s64;
                    } else {
                        quint64 u64 = scalar.toULongLong(&ok, base);

                        if (ok && (u64 <= std::numeric_limits<quint32>::max()))
                            val = quint32(u64);
                        else if (ok)
                            val = u64;
                    }
                }
                if (ok)
                    return val;
            }
        }
    }
    return scalar;
}

bool YamlParser::isMap() const
{
    return d->event.type == YAML_MAPPING_START_EVENT;
}

QString YamlParser::parseMapKey()
{
    if (isScalar()) {
        QVariant key = parseScalar();
        if (key.type() == QVariant::String)
            return key.toString();
    }
    throw YamlParserException(this, "Only strings are supported as mapping keys");
}

QVariantMap YamlParser::parseMap()
{
    if (!isMap())
        return QVariantMap {};

    QVariantMap map;
    while (true) {
        nextEvent(); // read key
        if (d->event.type == YAML_MAPPING_END_EVENT)
            return map;

        QString key = parseMapKey();
        if (map.contains(key))
            throw YamlParserException(this, "Found duplicate key '%1' in mapping").arg(key);

        nextEvent(); // read value
        QVariant value = parseVariant();

        map.insert(key, value);
    }
}

bool YamlParser::isList() const
{
    return d->event.type == YAML_SEQUENCE_START_EVENT;
}

void YamlParser::parseList(const std::function<void(YamlParser *p)> &callback)
{
    if (!isList())
        throw YamlParserException(this, "Cannot parse non-list as list");

    while (true) {
        nextEvent(); // read next value

        if (d->event.type == YAML_SEQUENCE_END_EVENT)
            return;

        switch (d->event.type) {
        case YAML_SCALAR_EVENT:
        case YAML_MAPPING_START_EVENT:
        case YAML_SEQUENCE_START_EVENT:
            callback(this);
            break;
        default:
            throw YamlParserException(this, "Unexpected event (%1) encountered while parsing a list").arg(d->event.type);
        }
    }
}

QVariant YamlParser::parseVariant()
{
    QVariant value;
    if (isScalar()) {
        value = parseScalar();
    } else if (isList()) {
        value = parseList();
    } else if (isMap()) {
        value = parseMap();
    } else {
        throw YamlParserException(this, "Unexpected event (%1) encountered while parsing a variant")
                .arg(d->event.type);
    }
    return value;
}

QVariantList YamlParser::parseList()
{
    QVariantList list;
    parseList([&list](YamlParser *p) {
        list.append(p->parseVariant());
    });
    return list;
}

QStringList YamlParser::parseStringOrStringList()
{
    if (isList()) {
        QStringList result;
        parseList([&result](YamlParser *p) {
            if (!p->isScalar())
                throw YamlParserException(p, "A list or map is not a valid member of a string-list");
            result.append(p->parseString());
        });
        return result;
    } else if (isScalar()) {
        return { parseString() };
    } else {
        throw YamlParserException(this, "Cannot parse a map as string or string-list");
    }
}

void YamlParser::parseFields(const std::vector<Field> &fields)
{
    if (!isMap())
        throw YamlParserException(this, "Expected a map (type %1) to parse fields from, but got type %2")
            .arg(YAML_MAPPING_START_EVENT).arg(d->event.type);

    QVector<QString> fieldsFound;

    while (true) {
        nextEvent(); // read key
        if (d->event.type == YAML_MAPPING_END_EVENT)
            break;
        QString key = parseMapKey();
        if (fieldsFound.contains(key))
            throw YamlParserException(this, "Found duplicate key '%1' in mapping").arg(key);

        auto field = fields.cbegin();
        for (; field != fields.cend(); ++field) {
            if (key == qL1S(field->name))
                break;
        }
        if (field == fields.cend())
            throw YamlParserException(this, "Field '%1' is not valid in this context").arg(key);
        fieldsFound << key;

        nextEvent(); // read value
        QVector<yaml_event_type_t> allowedEvents;
        if (field->types & YamlParser::Scalar)
            allowedEvents.append(YAML_SCALAR_EVENT);
        if (field->types & YamlParser::Map)
            allowedEvents.append(YAML_MAPPING_START_EVENT);
        if (field->types & YamlParser::List)
            allowedEvents.append(YAML_SEQUENCE_START_EVENT);

        if (!allowedEvents.contains(d->event.type)) { // ALIASES MISSING HERE!
            throw YamlParserException(this, "Field '%1' expected to have one of these types (%2), but got %3")
                            .arg(field->name).arg(allowedEvents).arg(d->event.type);
        }

        yaml_event_type_t typeBefore = d->event.type;
        yaml_event_type_t typeAfter;
        switch (typeBefore) {
        case YAML_MAPPING_START_EVENT: typeAfter = YAML_MAPPING_END_EVENT; break;
        case YAML_SEQUENCE_START_EVENT: typeAfter = YAML_SEQUENCE_END_EVENT; break;
        default: typeAfter = typeBefore; break;
        }
        field->callback(this);
        if (d->event.type != typeAfter) {
            throw YamlParserException(this, "Invalid YAML event state after field callback for '%3': expected %1, but got %2")
                .arg(typeAfter).arg(d->event.type).arg(key);
        }
    }
    QStringList fieldsMissing;
    for (auto field : fields) {
        if (field.required && !fieldsFound.contains(qL1S(field.name)))
            fieldsMissing.append(qL1S(field.name));
    }
    if (!fieldsMissing.isEmpty())
        throw YamlParserException(this, "Required field(s) '%1' are missing").arg(fieldsMissing);
}

YamlParserException::YamlParserException(YamlParser *p, const char *errorString)
    : Exception(Error::Parse, "YAML parse error")
{
    bool isProblem = p->d->parser.problem;
    yaml_mark_t &mark = isProblem ? p->d->parser.problem_mark : p->d->parser.mark;

    QString context = QTextDecoder(p->d->codec).toUnicode(p->d->data);
    int rpos = context.indexOf(qL1C('\n'), int(mark.index));
    int lpos = context.lastIndexOf(qL1C('\n'), int(mark.index));
    context = context.mid(lpos + 1, rpos == -1 ? context.size() : rpos - lpos - 1);

    m_errorString.append(qSL(" at line %1, column %2 [%3]").arg(mark.line + 1).arg(mark.column + 1).arg(context));
    if (isProblem)
        m_errorString.append(qSL(" (%1)").arg(QString::fromUtf8(p->d->parser.problem)));
    if (errorString)
        m_errorString.append(qSL(": %1").arg(qL1S(errorString)));
}

QT_END_NAMESPACE_AM
