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
#include <QtAppManCommon/qtappmancommonglobal.h>
#include <QtAppManCommon/exception.h>

QT_BEGIN_NAMESPACE_AM

class YamlParserPrivate;
class YamlParserException;
class YamlEmitterPrivate;


enum class YamlVersion {
    None = 0,
    V1_1 = 1,
    V1_2 = 2,
};


class Q_APPMANCOMMON_EXPORT YamlEmitter {
public:
    enum class Style { Flow, Block };

    static QByteArray fromVariantDocuments(const QVector<QVariant> &maps,
                                           Style style = Style::Block);
    static QByteArray fromVariantDocuments(const QVector<QVariant> &maps, YamlVersion version,
                                           Style style = Style::Block);
};


class Q_APPMANCOMMON_EXPORT YamlParser
{
public:
    YamlParser(const QByteArray &data, const QString &fileName = QString());
    // 'parseVersion' is used for non-versioned documents only - you should not need to use this
    // constructor except for parsing legacy YAML documents.
    YamlParser(const QByteArray &data, YamlVersion parseVersion,
               const QString &fileName = QString());
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

#if QT_AM_VERSION < QT_VERSION_CHECK(6, 13, 0)
    // for auto-test:
    static void disableDeprecationWarnings();
#endif

private:
    Q_DISABLE_COPY_MOVE(YamlParser)

    QString parseMapKey();
    static QVariant parseKeyword(const QString &str, YamlParser *parser,
                                 YamlVersion parseVersion = YamlVersion::None);

    YamlParserPrivate *d;
    friend class YamlParserException;
    friend class YamlEmitterPrivate;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(YamlParser::FieldTypes)

class Q_APPMANCOMMON_EXPORT YamlParserException : public Exception  // clazy:exclude=copyable-polymorphic
{
public:
    explicit YamlParserException(const YamlParser *p, const char *errorString);
};

QT_END_NAMESPACE_AM
// We mean it. Dummy comment since syncqt needs this also for completely private Qt modules.

#endif // QTYAML_H
