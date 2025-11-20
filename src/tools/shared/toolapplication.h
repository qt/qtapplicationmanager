// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#ifndef TOOLAPPLICATION_H
#define TOOLAPPLICATION_H

#include <QtAppManCommon/global.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QByteArray>
#include <QtCore/QException>

QT_FORWARD_DECLARE_CLASS(QCommandLineParser)

QT_BEGIN_NAMESPACE_AM

class ToolApplicationBase : public QCoreApplication
{
    Q_OBJECT

public:
    ToolApplicationBase(const char *name, int &argc, char **argv);

    template <typename T> void runLater(T slot)
    {
        // run the specified function as soon as the event loop is up and running
        QMetaObject::invokeMethod(this, slot, Qt::QueuedConnection);
    }

    int exec();

    QString parsePasswordOption(const QString &option, const QString &hint);

protected:
    void setCommands(const std::vector<std::tuple<uint, const char *, const char *>> &list);
    uint parse(QCommandLineParser &clp);

    bool notify(QObject *object, QEvent *event) override;

private:
    static void setName(const char *name);
    QString readPasswordFromConsole(const QString &hint);

    QString m_toolName;
    QByteArray m_description;
    std::vector<std::tuple<uint, const char *, const char *>> m_commands;
    std::unique_ptr<QException> m_exception;
};

template <typename CMDENUM, typename = std::is_same<uint, std::underlying_type<CMDENUM>>>
class ToolApplication : public ToolApplicationBase
{
public:
    using ToolApplicationBase::ToolApplicationBase;

    void setCommands(const std::vector<std::tuple<CMDENUM, const char *, const char *>> &list)
    {
        ToolApplicationBase::setCommands(*reinterpret_cast<const std::vector<
                                             std::tuple<uint, const char *, const char *>> *>(&list));
    }

    CMDENUM parse(QCommandLineParser &clp)
    {
        return static_cast<CMDENUM>(ToolApplicationBase::parse(clp));
    }
};

QT_END_NAMESPACE_AM

#endif // TOOLAPPLICATION_H
