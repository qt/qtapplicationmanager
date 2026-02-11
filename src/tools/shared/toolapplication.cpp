// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QCoreApplication>
#include <QCommandLineParser>

#include <QtAppManCommon/exception.h>
#include "toolapplication.h"

#include <iostream>
#if defined(Q_OS_UNIX)
#  include <unistd.h>
#  include <fcntl.h>
#  include <termios.h>
#elif defined(Q_OS_WINDOWS)
#  include <windows.h>
#endif

using namespace Qt::StringLiterals;

QT_BEGIN_NAMESPACE_AM

void ToolApplicationBase::setName(const char *name)
{
    // This needs to run before the QCoreApplication c'tor and we use the comma operator in
    // our derived class' c'tor to achieve that.
    QCoreApplication::setApplicationName(u"Qt ApplicationManager " + QString::fromLatin1(name));
    QCoreApplication::setOrganizationName(u"QtProject"_s);
    QCoreApplication::setOrganizationDomain(u"qt-project.org"_s);
    QCoreApplication::setApplicationVersion(QStringLiteral(QT_AM_VERSION_STR));
}

ToolApplicationBase::ToolApplicationBase(const char *name, int &argc, char **argv)
    : QCoreApplication((setName(name), argc), argv)
    , m_toolName(u"appman-"_s + QString::fromLatin1(name).toLower())
{ }

int ToolApplicationBase::exec()
{
    int r = QCoreApplication::exec();
    if (m_exception)
        m_exception->raise();
    return r;
}

void ToolApplicationBase::setCommands(const std::vector<std::tuple<uint, const char *, const char *>> &list)
{
    m_commands = list;
}

uint ToolApplicationBase::parse(QCommandLineParser &clp)
{
    clp.addHelpOption();
    clp.addVersionOption();

    clp.addPositionalArgument(u"command"_s, u"The command to execute."_s);

    // ignore unknown options for now -- the sub-commands may need them later
    clp.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsPositionalArguments);

    // ignore the return value here, as we also accept options we don't know about yet.
    // If an option is really not accepted by a command, the command specific parsing should report
    // this.
    clp.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);
    clp.parse(QCoreApplication::arguments());

    const QString descriptionTemplate =
        u"\n"_s + QCoreApplication::applicationName()
        + u"%1\n\nSee also https://doc.qt.io/QtApplicationManager/" + m_toolName + u".html";


    if (!clp.positionalArguments().isEmpty()) {
        QString cmd = clp.positionalArguments().at(0);

        for (const auto &[command, name, description] : m_commands) {
            if (cmd == QLatin1StringView(name)) {
                const QString commandDescription = u" >> %1"_s.arg(name);
                clp.setApplicationDescription(descriptionTemplate.arg(commandDescription));
                clp.clearPositionalArguments();
                clp.addPositionalArgument(cmd, QString::fromLatin1(description), cmd);
                return command;
            }
        }
    }

    if (clp.isSet(u"version"_s))
        clp.showVersion();

    QString commandsDescriptions = u"\n\nAvailable commands are:\n"_s;
    size_t longestName = 0;
    for (const auto &[command, name, description] : std::as_const(m_commands))
        longestName = std::max(longestName, qstrlen(name));
    for (const auto &[command, name, description] : std::as_const(m_commands)) {
        commandsDescriptions += u"  " + QString::fromLatin1(name)
                                + QString(1 + qsizetype(longestName - qstrlen(name)), u' ')
                                + QString::fromLatin1(description) + u'\n';
    }
    commandsDescriptions += u"\nMore information about each command can be obtained by running\n  "
                            + m_toolName + u" <command> --help";

    clp.setApplicationDescription(descriptionTemplate.arg(commandsDescriptions));
    if (clp.isSet(u"help"_s))
        clp.showHelp(0);
    clp.showHelp(1);
    return { };
}

QString ToolApplicationBase::parsePasswordOption(const QString &option, const QString &hint)
{
    // see: man openssl-passphrase-options
    // supported formats: pass:password, env:varname, file:filename, fd:number and stdin

    auto readPasswordFromFile = [](QFile &f) -> QString {
        auto pw = QString::fromLocal8Bit(f.readLine());
        while (pw.endsWith('\n') || pw.endsWith('\r'))
            pw.chop(1);
        return pw;
    };

    if (option.isEmpty()) {
        return { };
    } else if (option.startsWith(u"pass:"_s)) {
        return option.mid(5);
    } else if (option.startsWith(u"env:"_s)) {
        QByteArray env = option.mid(4).toLocal8Bit();
        if (!qEnvironmentVariableIsSet(env.constData()))
            throw Exception("Environment variable '%1' is not set").arg(env);
        return qEnvironmentVariable(env.constData());
    } else if (option.startsWith(u"file:"_s)) {
        QFile f(option.mid(5));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            throw Exception(f, "Could not open password file");
        return readPasswordFromFile(f);
#if defined(Q_OS_UNIX)
    } else if (option.startsWith(u"fd:"_s)) {
        bool ok = false;
        int fd = option.mid(3).toInt(&ok);
        if (!ok || (fd < 0))
            throw Exception("Could not parse file descriptor number from password option: %1").arg(option);
        QFile f;
        if (!f.open(fd, QIODevice::ReadOnly | QIODevice::Text))
            throw Exception(f, "Could not open file descriptor %1 for reading password").arg(fd);
        return readPasswordFromFile(f);
#endif
    } else if (option == u"stdin"_s) {
        return readPasswordFromConsole(hint + u": ");
    } else {
        throw Exception("Unknown password format. Needs to be in the form "
                        "pass:<password>, env:<envvar>, file:<path>, fd:<number> or stdin. "
                        "See the documentation for details.");
    }
}

bool ToolApplicationBase::notify(QObject *object, QEvent *event)
{
    try {
        return QCoreApplication::notify(object, event);
    } catch (const QException &e) {
        m_exception.reset(e.clone());
        exit(3);
        return true;
    }
}

QString ToolApplicationBase::readPasswordFromConsole(const QString &prompt)
{
    try {
#if defined(Q_OS_UNIX)
        int ttyFd = ::open("/dev/tty", O_RDWR | O_CLOEXEC);
        if (ttyFd < 0)
            throw Exception(errno, "Cannot open /dev/tty");

        auto cleanupTty = qScopeGuard([&] {
            ::close(ttyFd);
        });

        struct ::termios tcBefore;
        if (::tcgetattr(ttyFd, &tcBefore) != 0)
            throw Exception("Cannot get terminal attributes");

        struct ::termios tcPassword = tcBefore;
        tcPassword.c_lflag &= ~(ECHO | ECHONL);

#  ifndef TCSASOFT  // using this BSD optimization is recommended, if it's available
#    define TCSASOFT 0
#  endif

        auto cleanupTerm = qScopeGuard([&] {
            ::tcsetattr(ttyFd, TCSAFLUSH | TCSASOFT, &tcBefore);
        });

        if (::tcsetattr(ttyFd, TCSAFLUSH | TCSASOFT, &tcPassword) != 0)
            throw Exception("Cannot set terminal attributes");

        QByteArray baPrompt = prompt.toLocal8Bit();
        auto ignore [[maybe_unused]] = ::write(ttyFd, baPrompt.constData(), baPrompt.size());

        QByteArray password(1023, 0);
        int nread = ::read(ttyFd, password.data(), password.size());
        if (nread < 0)
            throw Exception("Error reading password from terminal");
        password.resize(nread);
        while (password.endsWith('\n') || password.endsWith('\r'))
            password.chop(1);

        ignore += ::write(ttyFd, "\n", 1);
        return QString::fromLocal8Bit(password);

#elif defined(Q_OS_WINDOWS)
        HANDLE hin = INVALID_HANDLE_VALUE;
        HANDLE hout = INVALID_HANDLE_VALUE;
        DWORD oldMode = 0;

        auto cleanup = qScopeGuard([&] {
            // calls on invalid handles are no-ops
            ::SetConsoleMode(hin, oldMode);
            ::CloseHandle(hin);
            ::CloseHandle(hout);
        });

        hin = ::CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (!::GetConsoleMode(hin, &oldMode))
            throw Exception("Cannot get console mode");
        DWORD mode = (oldMode | ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT) & ~ENABLE_ECHO_INPUT;
        if (!::SetConsoleMode(hin, mode))
            throw Exception("Cannot set console mode");

        hout = ::CreateFileA("CONOUT$", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (!::WriteConsoleW(hout, prompt.constData(), prompt.size(), nullptr, nullptr))
            throw Exception("Cannot write to console");

        static constexpr DWORD wbufLen = 1024;
        auto wbuf = new wchar_t[wbufLen];
        DWORD nread = 0;
        if (!::ReadConsoleW(hin, wbuf, wbufLen, &nread, nullptr)
            || (nread < 2)
            || (wbuf[nread - 2] != '\r')
            || (wbuf[nread - 1] != '\n')) {
            throw Exception("Failed to read from console");
        }
        wbuf[nread - 2] = 0; // remove \r\n
        ::WriteConsoleA(hout, "\n", 1, nullptr, nullptr);
        return QString::fromWCharArray(wbuf);

#endif
    } catch (const Exception &e) {
        std::cerr << "\nWarning: " << e.what() << "\n\n";
    }
    std::string line;
    std::cout << "[Password will be visible!] " << prompt.toLocal8Bit().constData();
    std::cin >> line;
    return QString::fromLocal8Bit(line).trimmed();
}

QT_END_NAMESPACE_AM
