/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtApplicationManager module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
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
****************************************************************************/

#include <QVariant>
#include <QRegularExpression>
#include <QDebug>
#include <QtNumeric>
#include <QFileInfo>
#include <QDir>

#include <yaml.h>

#include "global.h"
#include "qtyaml.h"
#include "exception.h"

QT_BEGIN_NAMESPACE_AM


namespace QtYaml {

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

    switch (value.metaType().id()) {
    default:
    case QMetaType::UnknownType:
        emitYamlScalar(e, "~");
        break;
    case QMetaType::Bool:
        emitYamlScalar(e, value.toBool() ? "true" : "false");
        break;
    case QMetaType::Int:
    case QMetaType::LongLong:
        emitYamlScalar(e, QByteArray::number(value.toLongLong()));
        break;
    case QMetaType::UInt:
    case QMetaType::ULongLong:
        emitYamlScalar(e, QByteArray::number(value.toULongLong()));
        break;
    case QMetaType::Double:
        emitYamlScalar(e, QByteArray::number(value.toDouble()));
        break;
    case QMetaType::QString:
        emitYamlScalar(e, value.toString().toUtf8(), true);
        break;
    case QMetaType::QVariantList:
    case QMetaType::QStringList: {
        yerr(yaml_sequence_start_event_initialize(&event, nullptr, nullptr, 1, style == FlowStyle ? YAML_FLOW_SEQUENCE_STYLE : YAML_BLOCK_SEQUENCE_STYLE));
        yerr(yaml_emitter_emit(e, &event));

        const QVariantList list = value.toList();
        for (const QVariant &listValue : list)
            emitYaml(e, listValue, style);

        yerr(yaml_sequence_end_event_initialize(&event));
        yerr(yaml_emitter_emit(e, &event));
        break;
    }
    case QMetaType::QVariantMap: {
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
    QString sourceName;
    QString sourceDir;
    QByteArray data;
    bool parsedHeader = false;
    yaml_parser_t parser;
    yaml_event_t event;

};


YamlParser::YamlParser(const QByteArray &data, const QString &fileName)
    : d(new YamlParserPrivate)
{
    d->data = data;
    if (!fileName.isEmpty()) {
        QFileInfo fi(fileName);
        d->sourceDir = fi.absolutePath();
        d->sourceName = fi.fileName();
    }

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
    case YAML_UTF8_ENCODING:    break;
    default:
    case YAML_UTF16LE_ENCODING:
    case YAML_UTF16BE_ENCODING:
        throw Exception("Only UTF-8 is supported as a YAML encoding");
    }
}

YamlParser::~YamlParser()
{
    if (d->event.type != YAML_NO_EVENT)
        yaml_event_delete(&d->event);
    yaml_parser_delete(&d->parser);
    delete d;
}

QString YamlParser::sourcePath() const
{
    return QDir(sourceDir()).absoluteFilePath(sourceName());
}

QString YamlParser::sourceDir() const
{
    return d->sourceDir;
}

QString YamlParser::sourceName() const
{
    return d->sourceName;
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
            throw YamlParserException(this, "invalid YAML syntax");
        if (d->event.type == YAML_ALIAS_EVENT)
            throw YamlParserException(this, "anchors and aliases are not supported");
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

    return QString::fromUtf8(reinterpret_cast<const char *>(d->event.data.scalar.value),
                             static_cast<int>(d->event.data.scalar.length));
}

QVariant YamlParser::parseScalar() const
{
    QString scalar = parseString();

    if (d->event.data.scalar.style == YAML_SINGLE_QUOTED_SCALAR_STYLE
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
        QVariant::fromValue(nullptr),  // ValueNull
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
        // We are using QRegularExpressions in multiple threads here, although the class is not
        // marked thread-safe. We are relying on the const match() function to behave thread-safe
        // which it does. There's an autotest (tst_yaml / parallel) to make sure we catch changes
        // in the internal implementation in the future.
        // The easiest way would be to deep-copy the objects into TLS instances, but
        // QRegularExpression is lacking such a functionality. Creating all the objects from
        // scratch in every thread is expensive though, so we count on match() being thread-safe.
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
        if (key.metaType() == QMetaType::fromType<QString>())
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

static QString mapEventNames(const QVector<yaml_event_type_t> &events)
{
    static const QHash<yaml_event_type_t, const char *> eventNames = {
        { YAML_NO_EVENT,             "nothing" },
        { YAML_STREAM_START_EVENT,   "stream start" },
        { YAML_STREAM_END_EVENT,     "stream end" },
        { YAML_DOCUMENT_START_EVENT, "document start" },
        { YAML_DOCUMENT_END_EVENT,   "document end" },
        { YAML_ALIAS_EVENT,          "alias" },
        { YAML_SCALAR_EVENT,         "scalar" },
        { YAML_SEQUENCE_START_EVENT, "sequence start" },
        { YAML_SEQUENCE_END_EVENT,   "sequence end" },
        { YAML_MAPPING_START_EVENT,  "mapping start" },
        { YAML_MAPPING_END_EVENT,    "mapping end" }
    };
    QString names;
    for (int i = 0; i < events.size(); ++i) {
        if (i)
            names.append(i == (events.size() - 1) ? qL1S(" or ") : qL1S(", "));
        names.append(qL1S(eventNames.value(events.at(i), "<unknown>")));
    }
    return names;
};

void YamlParser::parseFields(const std::vector<Field> &fields)
{
    QVector<QString> fieldsFound;

    if (!isMap()) {
        // an empty document is ok - we just have to check for required fields below
        if (!isScalar() || (parseScalar() != QVariant::fromValue(nullptr))) {
            throw YamlParserException(this, "Expected a map (type '%1') to parse fields from, but got type '%2'")
                .arg(mapEventNames({ YAML_MAPPING_START_EVENT })).arg(mapEventNames({ d->event.type }));
        }
    } else {
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
                throw YamlParserException(this, "Field '%1' expected to be of type '%2', but got '%3'")
                    .arg(field->name).arg(mapEventNames(allowedEvents)).arg(mapEventNames({ d->event.type }));
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
    }
    QStringList fieldsMissing;
    for (const auto &field : fields) {
        if (field.required && !fieldsFound.contains(qL1S(field.name)))
            fieldsMissing.append(qL1S(field.name));
    }
    if (!fieldsMissing.isEmpty())
        throw YamlParserException(this, "Required fields are missing: %1").arg(fieldsMissing);
}

YamlParserException::YamlParserException(YamlParser *p, const char *errorString)
    : Exception(Error::Parse, "YAML parse error")
{
    bool isProblem = p->d->parser.problem;
    yaml_mark_t &mark = isProblem ? p->d->parser.problem_mark : p->d->parser.mark;

    QString context = QString::fromUtf8(p->d->data);
    int lpos = context.lastIndexOf(qL1C('\n'), int(mark.index ? mark.index - 1 : 0));
    int rpos = context.indexOf(qL1C('\n'), int(mark.index));
    context = context.mid(lpos + 1, rpos == -1 ? context.size() : rpos - lpos - 1);
    int contextPos = int(mark.index) - (lpos + 1);

    m_errorString.append(qSL(":\nfile://%1:%2:%3: error").arg(p->sourcePath()).arg(mark.line + 1).arg(mark.column + 1));
    if (errorString)
        m_errorString.append(qSL(": %1").arg(qL1S(errorString)));
    if (isProblem)
        m_errorString.append(qSL(": %1").arg(QString::fromUtf8(p->d->parser.problem)));
    if (!context.isEmpty())
        m_errorString.append(qSL("\n %1\n %2^").arg(context, QString(contextPos, qL1C(' '))));
}

QT_END_NAMESPACE_AM
