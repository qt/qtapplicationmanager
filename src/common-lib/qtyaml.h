// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef QTYAML_H
#define QTYAML_H

#include <functional>
#include <vector>
#include <chrono>

#include <QtCore/QJsonParseError>
#include <QtCore/QVector>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtAppManCommon/global.h>
#include <QtAppManCommon/exception.h>

QT_BEGIN_NAMESPACE_AM

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

enum YamlStyle { FlowStyle, BlockStyle };

QByteArray yamlFromVariantDocuments(const QVector<QVariant> &maps, YamlStyle style = BlockStyle);

} // namespace QtYaml

class YamlParserPrivate;
class YamlParserException;

class YamlParser
{
public:
    YamlParser(const QByteArray &data, const QString &fileName = QString());
    ~YamlParser();

    QString sourceUrl() const;
    QString sourceDir() const;
    QString sourceName() const;

    static QVector<QVariant> parseAllDocuments(const QByteArray &yaml);

    QPair<QString, int> parseHeader();

    bool nextDocument();
    void nextEvent();

    bool isScalar() const;
    QVariant parseScalar();
    QString parseString();
    int parseInt(int min = std::numeric_limits<int>::min(), int max = std::numeric_limits<int>::max());
    bool parseBool();
    std::chrono::seconds parseDurationAsSec(QStringView defaultUnit = { });
    std::chrono::milliseconds parseDurationAsMSec(QStringView defaultUnit = { });
    std::chrono::microseconds parseDurationAsUSec(QStringView defaultUnit = { });

    bool isMap() const;
    QVariantMap parseMap();
    QMap<QString, QString> parseStringMap();

    bool isList() const;
    QVariantList parseList();
    void parseList(const std::function<void()> &callback);

    // convenience
    QVariant parseVariant();
    QStringList parseStringOrStringList();

    enum FieldType : int { Scalar = 0x01, List = 0x02, Map = 0x04 };
    Q_DECLARE_FLAGS(FieldTypes, FieldType)
    struct Field
    {
        QString name;
        bool required : 1;
        bool enabled : 1;
        FieldTypes types;
        std::function<void()> callback;

        Field(const char *_name, bool _required, FieldTypes _types,
              const std::function<void()> &_callback)
            : name(QString::fromLatin1(_name))
            , required(_required)
            , enabled(true)
            , types(_types)
            , callback(_callback)
        { }
        Field(bool _enabled, const char *_name, bool _required, FieldTypes _types,
              const std::function<void()> &_callback)
            : name(QString::fromLatin1(_name))
            , required(_required)
            , enabled(_enabled)
            , types(_types)
            , callback(_callback)
        { }
    };
    typedef std::vector<Field> Fields;

    void parseFields(const Fields &fields);

private:
    Q_DISABLE_COPY_MOVE(YamlParser)

    QString parseMapKey();

    YamlParserPrivate *d;
    friend class YamlParserException;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(YamlParser::FieldTypes)

class YamlParserException : public Exception  // clazy:exclude=copyable-polymorphic
{
public:
    explicit YamlParserException(YamlParser *p, const char *errorString);
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // QTYAML_H
