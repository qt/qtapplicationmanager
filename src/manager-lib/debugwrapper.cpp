// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "debugwrapper.h"

QT_BEGIN_NAMESPACE_AM

namespace DebugWrapper {

bool parseSpecification(const QString &debugWrapperSpecification, QStringList &resultingCommand,
                        QMap<QString, QString> &resultingEnvironment)
{
    QStringList cmd;
    QMap<QString, QString> env;

    // split at ' ', but also accept escape sequences for '\ ', '\n' and '\\'
    // all leading FOO=BAR args will be converted to env var assignments
    bool escaped = false;
    bool canBeEnvVar = true;
    bool isEnvVar = false;
    static const QString program = qSL("%program%");
    static const QString arguments = qSL("%arguments%");
    int foundProgram = 0;
    int foundArguments = 0;
    QString str;
    QString value;
    QString *current = &str;
    int size = debugWrapperSpecification.size();
    for (int i = 0; i <= size; ++i) {
        const QChar c = (i < size) ? debugWrapperSpecification.at(i) : QChar(0);
        if (!escaped || c.isNull()) {
            switch (c.unicode()) {
            case '\\':
                escaped = true;
                break;
            case '=':
                // switch to value parsing if this can be an env var and if this is the first =
                // all other = are copied verbatim
                isEnvVar = canBeEnvVar;
                if (isEnvVar && current == &str)
                    current = &value;
                else
                    current->append(qL1C('='));
                break;
            case ' ':
            case '\0':
                // found the end of an argument: add to env vars or cmd and reset state
                if (!str.isEmpty()) {
                    if (isEnvVar) {
                        env.insert(str, value);
                    } else {
                        cmd.append(str);

                        foundArguments = foundArguments || str.contains(arguments);
                        foundProgram = foundProgram || str.contains(program);
                    }
                }
                canBeEnvVar = isEnvVar; // stop parsing as envvar as soon as we get a normal arg
                isEnvVar = false;
                current = &str;
                str.clear();
                value.clear();
                break;
            default:
                current->append(c);
                break;
            }
        } else {
            switch (c.unicode()) {
            case '\\': current->append(qL1C('\\')); break;
            case ' ':  current->append(qL1C(' ')); break;
            case 'n':  current->append(qL1C('\n')); break;
            default:   return false;
            }
            escaped = false;
        }
    }

    if (cmd.isEmpty() && env.isEmpty())
        return false;

    // convenience: if the spec doesn't include %program% or %arguments%, then simply append them
    if (!foundProgram)
        cmd << program;
    if (!foundArguments)
        cmd << arguments;

    resultingCommand = cmd;
    resultingEnvironment = env;
    return true;
}

QStringList substituteCommand(const QStringList &debugWrapperCommand, const QString &program,
                              const QStringList &arguments)
{
    QString stringifiedArguments = arguments.join(qL1C(' '));
    QStringList result;

    for (const QString &s : debugWrapperCommand) {
        if (s == qSL("%arguments%")) {
            result << arguments;
        } else {
            QString str(s);
            str.replace(qL1S("%program%"), program);
            str.replace(qL1S("%arguments%"), stringifiedArguments);
            result << str;
        }
    }
    return result;
}

} // namespace DebugWrapper

QT_END_NAMESPACE_AM
