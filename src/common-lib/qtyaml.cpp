// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:data-parser

#include <QHash>
#include <QVariant>
#include <QRegularExpression>
#include <QDebug>
#include <QtNumeric>
#include <QFileInfo>
#include <QDir>
#include <QByteArray>
#include <QVersionNumber>

#include <yaml.h>

#include "qtyaml.h"
#include "exception.h"
#include "logging.h"

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

#if QT_AM_VERSION < QT_VERSION_CHECK(6, 13, 0)
static constexpr YamlVersion defaultVersion = YamlVersion::V1_1;

static bool showDeprecationWarnings = true;

void YamlParser::disableDeprecationWarnings()
{
    showDeprecationWarnings = false;
}

static void deprecationWarningHelper(const char *feature, const YamlParser *parser)
{
    // this is not ideal, but then again, this is a temporary measure
    QString s = YamlParserException(parser, nullptr).errorString();
    s.remove(u"YAML parse error:\n"_s);
    s.remove(u": error"_s);
    qCWarning(LogDeployment).noquote() << "Document is not explicitly versioned and YAML 1.2 deprecates"
                                       << feature << "at" << s;
}

#define deprecationWarning(condition, feature, parser)  \
    while (condition && parser && showDeprecationWarnings) { \
        deprecationWarningHelper(feature, parser); \
            break; \
    }

#else
static constexpr YamlVersion defaultVersion = YamlVersion::V1_2;
#define deprecationWarning(condition, feature, parser)
#endif


class YamlEmitterPrivate
{
public:
    YamlEmitterPrivate(YamlVersion version, YamlEmitter::Style style);

    QByteArray emitDocuments(const QVector<QVariant> &documents);

    inline void throwOnError(int result) noexcept(false)
    {
        if (!result)
            throw std::exception();
    }
    void emitVariant(const QVariant &value) noexcept(false);
    void emitScalar(const QByteArray &ba, bool quoting = false) noexcept(false);

private:
    YamlVersion m_version;
    YamlEmitter::Style m_style;
    yaml_emitter_t m_emitter;
};

YamlEmitterPrivate::YamlEmitterPrivate(YamlVersion version, YamlEmitter::Style style)
    : m_version((version == YamlVersion::None) ? defaultVersion : version)
    , m_style(style)
{ }

void YamlEmitterPrivate::emitScalar(const QByteArray &ba, bool quoting)
{
    yaml_event_t event;
    throwOnError(yaml_scalar_event_initialize(&event,
                                              nullptr,
                                              nullptr,
                                              reinterpret_cast<const yaml_char_t *>(ba.constData()),
                                              int(ba.size()),
                                              1,
                                              1,
                                              quoting ? YAML_SINGLE_QUOTED_SCALAR_STYLE
                                                      : YAML_ANY_SCALAR_STYLE));
    throwOnError(yaml_emitter_emit(&m_emitter, &event));
}

void YamlEmitterPrivate::emitVariant(const QVariant &value)
{
    yaml_event_t event;

    switch (value.metaType().id()) {
    case QMetaType::UnknownType:
    case QMetaType::Nullptr:
        emitScalar("~");
        break;
    case QMetaType::Bool:
        emitScalar(value.toBool() ? "true" : "false");
        break;
    case QMetaType::Int:
    case QMetaType::LongLong:
        emitScalar(QByteArray::number(value.toLongLong()));
        break;
    case QMetaType::UInt:
    case QMetaType::ULongLong:
        emitScalar(QByteArray::number(value.toULongLong()));
        break;
    case QMetaType::Double:
        emitScalar(QByteArray::number(value.toDouble()));
        break;
    default:
    case QMetaType::QString:
        emitScalar(value.toString().toUtf8(), true);
        break;
    case QMetaType::QVariantList:
    case QMetaType::QStringList: {
        throwOnError(yaml_sequence_start_event_initialize(&event, nullptr, nullptr, 1,
                                                          (m_style == YamlEmitter::Style::Flow)
                                                              ? YAML_FLOW_SEQUENCE_STYLE
                                                              : YAML_BLOCK_SEQUENCE_STYLE));
        throwOnError(yaml_emitter_emit(&m_emitter, &event));

        const QVariantList list = value.toList();
        for (const QVariant &listValue : list)
            emitVariant(listValue);

        throwOnError(yaml_sequence_end_event_initialize(&event));
        throwOnError(yaml_emitter_emit(&m_emitter, &event));
        break;
    }
    case QMetaType::QVariantMap: {
        throwOnError(yaml_mapping_start_event_initialize(&event, nullptr, nullptr, 1,
                                                         (m_style == YamlEmitter::Style::Flow)
                                                             ? YAML_FLOW_MAPPING_STYLE
                                                             : YAML_BLOCK_MAPPING_STYLE));
        throwOnError(yaml_emitter_emit(&m_emitter, &event));

        QVariantMap map = value.toMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            const QString &key = it.key();
            // We could just quote everything, but this would break backwards compatibility
            // inside the AM itself (e.g. HMAC calculations for installation-report.yaml)
            bool needsQuoting = key.isEmpty()
                                || YamlParser::parseKeyword(key, nullptr, m_version).isValid();
            if (!needsQuoting) {
                char16_t firstChar = key.at(0).unicode();
                needsQuoting = ((firstChar >= u'0' && firstChar <= u'9')
                                || firstChar == u'+' || firstChar == u'-' || firstChar == u'.');
            }
            emitScalar(key.toUtf8(), needsQuoting);
            emitVariant(it.value());
        }

        throwOnError(yaml_mapping_end_event_initialize(&event));
        throwOnError(yaml_emitter_emit(&m_emitter, &event));
        break;
    }
    }
}


QByteArray YamlEmitterPrivate::emitDocuments(const QVector<QVariant> &documents)
{
    QByteArray out;

    yaml_emitter_initialize(&m_emitter);
    yaml_emitter_set_output(&m_emitter, [] (void *data, unsigned char *buffer, size_t size) {
        static_cast<QByteArray *>(data)->append(reinterpret_cast<char *>(buffer), int(size));
        return 1;
    }, &out);

    yaml_emitter_set_width(&m_emitter, 80);
    yaml_emitter_set_indent(&m_emitter, 2);

    try {
        yaml_event_t event;
        throwOnError(yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING));
        throwOnError(yaml_emitter_emit(&m_emitter, &event));

        bool first = true;
        Q_ASSERT(m_version != YamlVersion::None); // set in the constructor
        yaml_version_directive_t yamlVersion { 1, int(m_version) };

        for (const QVariant &doc : documents) {
            throwOnError(yaml_document_start_event_initialize(&event, first ? &yamlVersion : nullptr,
                                                              nullptr, nullptr, 0));
            throwOnError(yaml_emitter_emit(&m_emitter, &event));

            emitVariant(doc);

            throwOnError(yaml_document_end_event_initialize(&event, 1));
            throwOnError(yaml_emitter_emit(&m_emitter, &event));

            first = false;
        }

        throwOnError(yaml_stream_end_event_initialize(&event));
        throwOnError(yaml_emitter_emit(&m_emitter, &event));
    } catch (const std::exception &) {
        out.clear();
    }
    yaml_emitter_delete(&m_emitter);

    return out;
}

QByteArray YamlEmitter::fromVariantDocuments(const QVector<QVariant> &documents,
                                             YamlVersion version, Style style)
{
    return YamlEmitterPrivate(version, style).emitDocuments(documents);
}

QByteArray YamlEmitter::fromVariantDocuments(const QVector<QVariant> &documents, Style style)
{
    return fromVariantDocuments(documents, YamlVersion::None, style);
}


class YamlParserPrivate
{
public:
    QString sourceName;
    QString sourceDir;
    QByteArray data;
    bool parsedHeader = false;
    yaml_parser_t parser;
    yaml_event_t event;
    YamlVersion documentVersion = YamlVersion::None;
    YamlVersion parseVersion = YamlVersion::None;
};

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
            names.append(i == (events.size() - 1) ? u" or " : u", ");
        names.append(QString::fromLatin1(eventNames.value(events.at(i), "<unknown>")));
    }
    return names;
}


YamlParser::YamlParser(const QByteArray &data, YamlVersion parseVersion, const QString &fileName)
    : d(new YamlParserPrivate)
{
    d->parseVersion = parseVersion;

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
    yaml_parser_set_input_string(&d->parser,
                                 reinterpret_cast<const unsigned char *>(d->data.constData()),
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

YamlParser::YamlParser(const QByteArray &data, const QString &fileName)
    : YamlParser(data, YamlVersion::None, fileName)
{ }

YamlParser::~YamlParser()
{
    if (d->event.type != YAML_NO_EVENT)
        yaml_event_delete(&d->event);
    yaml_parser_delete(&d->parser);
    delete d;
}

QString YamlParser::sourceUrl() const
{
    if (sourceName().isEmpty())
        return u"<yaml file>"_s;
    const QString s = QDir(sourceDir()).absoluteFilePath(sourceName());
    if (s.startsWith(u":/"))
        return u"qrc://" + s.mid(2);
    else
        return u"file://" + s;
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

std::pair<QString, int> YamlParser::parseHeader()
{
    if (d->parsedHeader)
        throw Exception("Already parsed header");
    if (d->event.type != YAML_STREAM_START_EVENT)
        throw Exception("Header must be the first document of a file");
    if (!nextDocument())
        throw Exception("No header document available");
    if (!isMap())
        throw Exception("Header document is not a map");

    std::pair<QString, int> result;

    const Fields fields = {
        { "formatType", true, YamlParser::Scalar, [&, this]() {
             result.first = parseScalar().toString(); } },
        { "formatVersion", true, YamlParser::Scalar, [&, this]() {
              result.second = parseScalar().toInt(); } }
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
            if (d->event.data.document_start.version_directive) {
                // YAML version directive found
                QVersionNumber v{d->event.data.document_start.version_directive->major,
                                 d->event.data.document_start.version_directive->minor};
                if (v < QVersionNumber(1, 1) || v > QVersionNumber(1, 2)) {
                    throw YamlParserException(this, "Only YAML versions 1.1 and 1.2 are supported (found %1)")
                        .arg(v.toString());
                }
                d->documentVersion = (v.minorVersion() == 1) ? YamlVersion::V1_1 : YamlVersion::V1_2;
            }
            if (d->documentVersion == YamlVersion::None) {
                // No version directive found, use the default version
                if (d->parseVersion == YamlVersion::None)
                    d->parseVersion = defaultVersion;
            } else {
                d->parseVersion = d->documentVersion; // Version directive found, use it
            }
            Q_ASSERT(d->parseVersion != YamlVersion::None);

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

QString YamlParser::parseString()
{
    if (!isScalar())
        throw YamlParserException(this, "expected a string value");

    Q_ASSERT(d->event.data.scalar.value);
    Q_ASSERT(static_cast<int>(d->event.data.scalar.length) >= 0);

    return QString::fromUtf8(reinterpret_cast<const char *>(d->event.data.scalar.value),
                             static_cast<int>(d->event.data.scalar.length));
}

int YamlParser::parseInt(int min, int max)
{
    auto v = parseScalar();
    if (v.typeId() != QMetaType::Int)
        throw YamlParserException(this, "expected an integer value");
    int i = v.toInt();
    if ((min != std::numeric_limits<int>::min()) && (i < min))
        throw YamlParserException(this, "integer value out of bounds: %1 < %2").arg(i).arg(min);
    if ((max != std::numeric_limits<int>::max()) && (i > max))
        throw YamlParserException(this, "integer value out of bounds: %1 > %2").arg(i).arg(max);
    return i;
}

bool YamlParser::parseBool()
{
    auto v = parseScalar();
    if (v.typeId() != QMetaType::Bool)
        throw YamlParserException(this, "expected a boolean value");
    return v.toBool();
}

std::chrono::seconds YamlParser::parseDurationAsSec(QStringView defaultUnit)
{
    return std::chrono::duration_cast<std::chrono::seconds>(parseDurationAsUSec(defaultUnit));
}

std::chrono::milliseconds YamlParser::parseDurationAsMSec(QStringView defaultUnit)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(parseDurationAsUSec(defaultUnit));
}

std::chrono::microseconds YamlParser::parseDurationAsUSec(QStringView defaultUnit)
{
    QString s = parseString().trimmed();
    if (s == u"off")
        return std::chrono::milliseconds::zero();

    // YAML allows _ as a grouping separator
    if (s.contains(u'_'))
        s = s.replace(u'_', u""_s);

    // std::from_chars would be ideal, but clang/libc++ don't implement the floating-point version
    qsizetype unitPos = 0;
    for ( ; unitPos < s.size(); ++unitPos) {
        const auto c = s.at(unitPos);
        if (!c.isDigit() && (c != u'.') && (c != u'-'))
            break;
    }

    double value = 0;
    bool ok = false;
    if (unitPos > 0)
        value = QStringView(s).left(unitPos).toDouble(&ok);

    if (!ok)
        throw YamlParserException(this, "could not parse as a time duration");

    QStringView unit = QStringView(s).mid(unitPos).trimmed();
    if (unit.isEmpty()) {
        if (defaultUnit.isEmpty())
            throw YamlParserException(this, "time duration requires a unit");
        unit = defaultUnit;
    }

    qint64 f;

    if (unit == u"h")
        f = 1LL * 1000 * 1000 * 60 * 60;
    else if (unit == u"min")
        f = 1LL * 1000 * 1000 * 60;
    else if (unit == u"s")
        f = 1LL * 1000 * 1000;
    else if (unit == u"ms")
        f = 1LL * 1000;
    else if (unit == u"us")
        f = 1LL;
    else
        throw YamlParserException(this, "unknown time duration unit (can be: h,min,s,ms,us)");

    return std::chrono::microseconds(qint64(value * f));
}

QVariant YamlParser::parseKeyword(const QString &str, YamlParser *parser, YamlVersion parseVersion)
{
    // This function is static to allow the Emitter to query for keywords as well.
    // - if called by the parser, 'parser' is 'this' and 'parseVersion' is ignored, because it is
    //   supplied via 'parser'.
    // - if called by the emitter, 'parser' is null and 'parseVersion' sets the version to use.

    // QVariant { } is also 'null', but NOT 'valid'
    static const auto vnull = QVariant::fromValue(nullptr);

    const static QHash<QString, std::pair<QVariant, qint8>> mappings {
        { u".INF"_s,  { qInf(),  1 | 2 } },
        { u".Inf"_s,  { qInf(),  1 | 2 } },
        { u".inf"_s,  { qInf(),  1 | 2 } },
        { u"+.INF"_s, { qInf(),  1 | 2 } },
        { u"+.Inf"_s, { qInf(),  1 | 2 } },
        { u"+.inf"_s, { qInf(),  1 | 2 } },
        { u"-.INF"_s, { -qInf(), 1 | 2 } },
        { u"-.Inf"_s, { -qInf(), 1 | 2 } },
        { u"-.inf"_s, { -qInf(), 1 | 2 } },
        { u".NAN"_s,  { qQNaN(), 1 | 2 } },
        { u".NaN"_s,  { qQNaN(), 1 | 2 } },
        { u".nan"_s,  { qQNaN(), 1 | 2 } },
        { u"FALSE"_s, { false,   1 | 2 } },
        { u"False"_s, { false,   1 | 2 } },
        { u"false"_s, { false,   1 | 2 } },
        { u"TRUE"_s,  { true,    1 | 2 } },
        { u"True"_s,  { true,    1 | 2 } },
        { u"true"_s,  { true,    1 | 2 } },
        { u"NULL"_s,  { vnull,   1 | 2 } },
        { u"Null"_s,  { vnull,   1 | 2 } },
        { u"null"_s,  { vnull,   1 | 2 } },
        { u"~"_s,     { vnull,   1 | 2 } },
        { u"N"_s,     { false,   1 } },
        { u"n"_s,     { false,   1 } },
        { u"NO"_s,    { false,   1 } },
        { u"No"_s,    { false,   1 } },
        { u"no"_s,    { false,   1 } },
        { u"OFF"_s,   { false,   1 } },
        { u"Off"_s,   { false,   1 } },
        { u"off"_s,   { false,   1 } },
        { u"ON"_s,    { true,    1 } },
        { u"On"_s,    { true,    1 } },
        { u"on"_s,    { true,    1 } },
        { u"Y"_s,     { true,    1 } },
        { u"y"_s,     { true,    1 } },
        { u"YES"_s,   { true,    1 } },
        { u"Yes"_s,   { true,    1 } },
        { u"yes"_s,   { true,    1 } }
    };

    static constexpr qsizetype longest = 5;

    if (auto l = str.length(); (l == 0) || (l > longest)) // early out to avoid expensive hashing
        return { };

    const int v = ((parser ? parser->d->parseVersion : parseVersion) == YamlVersion::V1_1) ? 1 : 2;

    if (auto it = mappings.constFind(str); it != mappings.cend()) {
        // we parse an implicitly 1.1 versioned doc, but the keyword is gone in 1.2
        deprecationWarning((parser && (parser->d->documentVersion == YamlVersion::None)
                            && (v == 1) && !(it->second & 2)), "keyword", parser);

        if (it->second & v)  // check if the keyword is valid in the current version
            return it->first;
    }
    return { };
}

QVariant YamlParser::parseScalar()
{
    if (!isScalar())
        throw YamlParserException(this, "Cannot parse %1 as scalar").arg(mapEventNames({ d->event.type }));
    QString scalar = parseString();

    const auto scalarAsKeyword = [&]() -> QVariant {
        if (scalar.isEmpty())
            return QVariant::fromValue(nullptr);
        if (auto v = parseKeyword(scalar, this); v.isValid())
            return v;
        return { };
    };

    // We are using QRegularExpressions in multiple threads here, although the class is not
    // marked thread-safe. We are relying on the const match() function to behave thread-safe
    // which it does. There's an autotest (tst_yaml / parallel) to make sure we catch changes
    // in the internal implementation in the future.
    // The easiest way would be to deep-copy the objects into TLS instances, but
    // QRegularExpression is lacking such a functionality. Creating all the objects from
    // scratch in every thread is expensive though, so we count on match() being thread-safe.

    auto scalarAsFloat = [&]() -> QVariant {
        if (scalar.isEmpty())
            return { };
        char16_t firstChar = scalar.at(0).unicode();
        if ((firstChar >= u'0' && firstChar <= u'9')   // cheap check to avoid expensive regexps
                || firstChar == u'+' || firstChar == u'-' || firstChar == u'.') {
            // The regexp from the 1.1 standard is broken, as not even '0' matches it, although
            // the spec lists that as a valid float. We always use the 1.2 version, which is able
            // to match decimal integers as well.
            //static const QRegularExpression floatRegExp11(u"\\A[-+]?([0-9][0-9_]*)?\\.[0-9.]*([eE][-+][0-9]+)?\\z"_s);
            static const QRegularExpression floatRegExp12(uR"(\A[-+]?(\.[0-9]+|[0-9]+(\.[0-9]*)?)([eE][-+]?[0-9]+)?\z)"_s);

            auto &regExp = floatRegExp12;

            if (regExp.match(scalar).hasMatch()) {
                QString numberStr = scalar;

                // YAML 1.1 allows _ as a grouping separator
                if (d->parseVersion == YamlVersion::V1_1) {
                    if (numberStr.contains(u'_')) {
                        numberStr = numberStr.replace(u'_', u""_s);

                        deprecationWarning(d->documentVersion == YamlVersion::None,
                                           "number grouping", this);
                    }
                }

                bool ok = false;
                if (auto d = numberStr.toDouble(&ok); ok)
                    return d;
            }
        }
        return { };
    };

    auto scalarAsInt = [&]() -> QVariant {
        if (scalar.isEmpty())
            return { };
        char16_t firstChar = scalar.at(0).unicode();
        if ((firstChar >= u'0' && firstChar <= u'9')   // cheap check to avoid expensive regexps
            || firstChar == u'+' || firstChar == u'-') {
            static const std::array<std::tuple<int, int, QRegularExpression>, 4> numberRegExps11 {{
                { 10, 0, QRegularExpression(u"\\A[-+]?(0|[1-9][0-9_]*)\\z"_s) }, // decimal
                { 16, 2, QRegularExpression(u"\\A[-+]?0x[0-9a-fA-F_]+\\z"_s) },  // hexadecimal
                {  2, 2, QRegularExpression(u"\\A[-+]?0b[0-1_]+\\z"_s) },        // binary
                {  8, 1, QRegularExpression(u"\\A[-+]?0[0-7_]+\\z"_s) },         // octal
            }};
            static const std::array<std::tuple<int, int, QRegularExpression>, 4> numberRegExps12 {{
                { 10, 0, QRegularExpression(uR"(\A[-+]?[0-9]+\z)"_s) },    // decimal
                { 16, 2, QRegularExpression(uR"(\A0x[0-9a-fA-F]+\z)"_s) }, // hexadecimal
                {  8, 2, QRegularExpression(uR"(\A0o[0-7]+\z)"_s) },       // octal
                { -1, 0, { } }, // dummy value to keep the array sizes the same
            }};

            const auto &numberRegExps = (d->parseVersion == YamlVersion::V1_2) ? numberRegExps12
                                                                               : numberRegExps11;

            for (const auto &[base, offset, regExp] : numberRegExps) {
                if (base < 0)
                    continue;

                if (regExp.match(scalar).hasMatch()) {
                    bool ok = false;
                    QVariant val;
                    QString numberStr = offset ? scalar.mid(offset) : scalar;

                    if (d->parseVersion == YamlVersion::V1_1) {
                        // YAML 1.1 allows _ as a grouping separator
                        if (numberStr.contains(u'_')) {
                            numberStr = numberStr.replace(u'_', u""_s);

                            deprecationWarning(d->documentVersion == YamlVersion::None,
                                               "number grouping", this);
                        }

                        if (d->documentVersion == YamlVersion::None) {
                            deprecationWarning((base != 10) && !scalar.startsWith(u'0'),
                                               "signed non-decimal numbers", this);
                            deprecationWarning(base == 8,
                                               "octal encoded nummbers with just a '0' prefix", this);
                            deprecationWarning(base == 2, "binary encoded numbers", this);
                        }
                    }

                    qint64 s64 = numberStr.toLongLong(&ok, base);
                    if (ok
                        && (s64 <= std::numeric_limits<qint32>::max())
                        && (s64 >= std::numeric_limits<qint32>::min())) {
                        return qint32(s64);
                    } else if (ok) {
                        return s64;
                    } else {
                        quint64 u64 = numberStr.toULongLong(&ok, base);
                        if (ok)
                            return u64;
                    }
                }
            }
        }
        return { };
    };

    if (d->event.data.scalar.tag) {
        auto tag = QByteArrayView(d->event.data.scalar.tag);
        if (!tag.startsWith("tag:yaml.org,2002:"))
            throw YamlParserException(this, "unsupported tag: %1").arg(QString::fromUtf8(tag));
        tag.slice(18);

        static const QHash<QByteArray, QMetaType::Type> knownTags = {
            { "str",   QMetaType::QString },
            { "int",   QMetaType::LongLong },
            { "float", QMetaType::Double },
            { "bool",  QMetaType::Bool },
            { "null",  QMetaType::Nullptr }
        };

        switch (knownTags.value(tag, QMetaType::UnknownType)) {
        case QMetaType::QString:
            return scalar;
        case QMetaType::LongLong:
            if (QVariant v = scalarAsInt(); v.isValid())
                return v;
            throw YamlParserException(this, "expected an integer value");
        case QMetaType::Double:
            if (QVariant v = scalarAsFloat(); v.isValid())
                return v;
            else if (QVariant v = scalarAsKeyword(); (v.isValid() && (v.userType() == QMetaType::Double)))
                return v;
            throw YamlParserException(this, "expected a float value");
        case QMetaType::Bool:
            if (QVariant v = scalarAsKeyword(); (v.isValid() && (v.userType() == QMetaType::Bool)))
                return v;
            throw YamlParserException(this, "expected a boolean value");
        case QMetaType::Nullptr:
            if (QVariant v = scalarAsKeyword(); (v.isValid() && (v.userType() == QMetaType::Nullptr)))
                return v;
            throw YamlParserException(this, "expected a null value");
        default:
            throw YamlParserException(this, "unsupported tag: tag:yaml.org,2002:%1").arg(QString::fromUtf8(tag));
        }
    } else if (d->event.data.scalar.style == YAML_SINGLE_QUOTED_SCALAR_STYLE
               || d->event.data.scalar.style == YAML_DOUBLE_QUOTED_SCALAR_STYLE) {
        return scalar;
    } else if (QVariant v = scalarAsKeyword(); v.isValid()) {
        return v;
    } else if (QVariant v = scalarAsInt(); v.isValid()) {
        return v;
    } else if (QVariant v = scalarAsFloat(); v.isValid()) {
        return v;
    } else {
        return scalar;
    }
}

bool YamlParser::isMap() const
{
    return d->event.type == YAML_MAPPING_START_EVENT;
}

QString YamlParser::parseMapKey()
{
    if (isScalar()) {
        QVariant key = parseScalar();
        if (key.typeId() == QMetaType::QString)
            return key.toString();
    }
    throw YamlParserException(this, "Only strings are supported as mapping keys");
}

QVariantMap YamlParser::parseMap()
{
    if (!isMap())
        throw YamlParserException(this, "Cannot parse %1 as map").arg(mapEventNames({ d->event.type }));

    QVariantMap map;
    while (true) {
        nextEvent(); // read key
        if (d->event.type == YAML_MAPPING_END_EVENT)
            return map;

        const QString key = parseMapKey();
        if (map.contains(key))
            throw YamlParserException(this, "Found duplicate key '%1' in mapping").arg(key);

        nextEvent(); // read value
        const QVariant value = parseVariant();

        map.insert(key, value);
    }
}

QMap<QString, QString> YamlParser::parseStringMap()
{
    if (!isMap())
        throw YamlParserException(this, "Cannot parse %1 as map").arg(mapEventNames({ d->event.type }));

    QMap<QString, QString> map;
    while (true) {
        nextEvent(); // read key
        if (d->event.type == YAML_MAPPING_END_EVENT)
            return map;

        const QString key = parseMapKey();
        if (map.contains(key))
            throw YamlParserException(this, "Found duplicate key '%1' in mapping").arg(key);

        nextEvent(); // read value
        const QString value = parseString();
        map.insert(key, value);
    }
}

bool YamlParser::isList() const
{
    return d->event.type == YAML_SEQUENCE_START_EVENT;
}

void YamlParser::parseList(const std::function<void()> &callback)
{
    if (!isList())
        throw YamlParserException(this, "Cannot parse %1 as list").arg(mapEventNames({ d->event.type }));

    while (true) {
        nextEvent(); // read next value

        if (d->event.type == YAML_SEQUENCE_END_EVENT)
            return;

        switch (d->event.type) {
        case YAML_SCALAR_EVENT:
        case YAML_MAPPING_START_EVENT:
        case YAML_SEQUENCE_START_EVENT:
            callback();
            break;
        default:
            throw YamlParserException(this, "Unexpected event (%1) encountered while parsing a list").arg(mapEventNames({ d->event.type }));
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
            .arg(mapEventNames({ d->event.type }));
    }
    return value;
}

QVariantList YamlParser::parseList()
{
    QVariantList list;
    parseList([this, &list]() {
        list.append(parseVariant());
    });
    return list;
}

QStringList YamlParser::parseStringOrStringList()
{
    if (isList()) {
        QStringList result;
        parseList([this, &result]() {
            if (!isScalar())
                throw YamlParserException(this, "A list or map is not a valid member of a string-list");
            result.append(parseString());
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
            const QString key = parseMapKey();
            if (fieldsFound.contains(key))
                throw YamlParserException(this, "Found duplicate key '%1' in mapping").arg(key);

            auto field = fields.cbegin();
            for (; field != fields.cend(); ++field) {
                if (key == field->name)
                    break;
            }
            if ((field == fields.cend()) || !field->enabled)
                throw YamlParserException(this, "Field '%1' is not valid in this context").arg(key);
            fieldsFound << key;

            nextEvent(); // read value

            static const auto eventAsFieldType = [](yaml_event_type_t type) -> FieldType {
                switch (type) {
                case YAML_SCALAR_EVENT:         return YamlParser::Scalar;
                case YAML_MAPPING_START_EVENT:  return YamlParser::Map;
                case YAML_SEQUENCE_START_EVENT: return YamlParser::List;
                default:                        return FieldType(0);
                }
            };

            if (!(field->types & eventAsFieldType(d->event.type))) { // ALIASES MISSING HERE!
                QVector<yaml_event_type_t> allowedEvents;
                if (field->types & YamlParser::Scalar)
                    allowedEvents.append(YAML_SCALAR_EVENT);
                if (field->types & YamlParser::Map)
                    allowedEvents.append(YAML_MAPPING_START_EVENT);
                if (field->types & YamlParser::List)
                    allowedEvents.append(YAML_SEQUENCE_START_EVENT);
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
            field->callback();
            if (d->event.type != typeAfter) {
                throw YamlParserException(this, "Invalid YAML event state after field callback for '%3': expected %1, but got %2")
                    .arg(mapEventNames({ typeAfter })).arg(mapEventNames({ d->event.type })).arg(key);
            }
        }
    }
    QStringList fieldsMissing;
    for (const auto &field : fields) {
        if (field.required && field.enabled && !fieldsFound.contains(field.name))
            fieldsMissing.append(field.name);
    }
    if (!fieldsMissing.isEmpty())
        throw YamlParserException(this, "Required fields are missing: %1").arg(fieldsMissing);
}

YamlParserException::YamlParserException(const YamlParser *p, const char *errorString)
    : Exception(Error::Parse, "YAML parse error")
{
    bool isProblem = p->d->parser.problem;
    yaml_mark_t &mark = isProblem ? p->d->parser.problem_mark : p->d->event.start_mark;
    QString context = QString::fromUtf8(p->d->data);
    qsizetype lpos = context.lastIndexOf(u'\n', qsizetype(mark.index ? mark.index - 1 : 0));
    qsizetype rpos = context.indexOf(u'\n', qsizetype(mark.index));
    context = context.mid(lpos + 1, rpos == -1 ? context.size() : rpos - lpos - 1);
    qsizetype contextPos = qsizetype(mark.index) - (lpos + 1);

    m_errorString.append(u":\n%1:%2:%3: error"_s.arg(p->sourceUrl()).arg(mark.line + 1).arg(mark.column + 1));
    if (errorString)
        m_errorString.append(u": %1"_s.arg(QString::fromLatin1(errorString)));
    if (isProblem)
        m_errorString.append(u": %1"_s.arg(QString::fromUtf8(p->d->parser.problem)));
    if (!context.isEmpty())
        m_errorString.append(u"\n %1\n %2^"_s.arg(context, QString(contextPos, u' ')));
}


QT_END_NAMESPACE_AM
