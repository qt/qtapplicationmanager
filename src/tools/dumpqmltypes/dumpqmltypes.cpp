/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtAppManInstaller/applicationinstaller.h>
#include <QtAppManManager/applicationmanager.h>
#include <QtAppManManager/applicationipcmanager.h>
#include <QtAppManManager/applicationipcinterface.h>
#include <QtAppManApplication/application.h>
#include <QtAppManManager/abstractruntime.h>
#include <QtAppManManager/abstractcontainer.h>
#include <QtAppManManager/notificationmanager.h>
#include <QtAppManNotification/notification.h>
#include <QtAppManManager/notificationmanager.h>
#include <QtAppManManager/qmlinprocessapplicationinterface.h>
#include <QtAppManWindow/windowmanager.h>
#include <QtAppManLauncher/qmlapplicationinterface.h>
#include <QtAppManLauncher/qmlapplicationinterfaceextension.h>
#include <QtAppManLauncher/private/applicationmanagerwindow_p.h>
#include <QtAppManCommon/global.h>

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <stdio.h>

QT_USE_NAMESPACE_AM

static QByteArray qmlTypeForMetaObect(const QMetaObject *mo, int level, bool indentFirstLine)
{
    static auto stripNamespace = [](const QByteArray &identifier) -> QByteArray {
        int pos = identifier.lastIndexOf("::");
        return pos < 0 ? identifier : identifier.mid(pos + 2);
    };
    static auto mapTypeName = [](const QByteArray &type, bool stripPointer) -> QByteArray {
        if (type == "QString") {
            return "string";
        } else if (type == "qlonglong") {
            return "int";
        } else {
            QByteArray ba = type;
            if (ba.startsWith("const "))
                ba = ba.mid(6);
            ba = stripNamespace(ba);
            if (stripPointer && ba.endsWith('*'))
                ba.chop(1);
            return ba;
        }
    };

    QByteArray str;
    QByteArray indent1 = QByteArray(level * 4, ' ');
    QByteArray indent2 = QByteArray((level + 1) * 4, ' ');
    QByteArray indent3 = QByteArray((level + 2) * 4, ' ');

    if (indentFirstLine)
        str.append(indent1);

    str = str + "Component {\n";
    str = str
        + indent2
        + "name: \""
        + stripNamespace(mo->className())
        + "\"\n";

    if (mo->superClass()) {
        str = str
            + indent2
            + "prototype: \""
            + stripNamespace(mo->superClass()->className())
            + "\"\n";
    }

    for (int i = mo->classInfoOffset(); i < mo->classInfoCount(); ++i) {
        QMetaClassInfo c = mo->classInfo(i);
        if (qstrcmp(c.name(), "AM-QmlType") == 0) {
            QByteArray type = c.value();
            int rev = type.mid(type.lastIndexOf('.')).toInt();

            str = str + indent2 + "exports: [ \"" + type + "\" ]\n";
            str = str + indent2 + "exportMetaObjectRevisions: [ " + QByteArray::number(rev) + " ]\n";
            break;
        }
    }

    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        QMetaProperty p = mo->property(i);
        str = str
            + indent2
            + "Property { name: \"" + p.name()
            + "\"; type: \"" + mapTypeName(p.typeName(), true) + "\";";
        if (QByteArray(p.typeName()).endsWith('*'))
            str += " isPointer: true;";
        if (!p.isWritable())
            str += " isReadonly: true";
        str = str + " }\n";
    }

    for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
        QMetaMethod m = mo->method(i);

        // suppress D-Bus interfaces
        if (isupper(m.name().at(0)))
            continue;

        QByteArray methodtype;
        if (m.methodType() == QMetaMethod::Signal) {
            methodtype = "Signal";
            if (m.access() == QMetaMethod::Private)
                continue;
        } else {
            if (m.access() != QMetaMethod::Public)
                continue;
            methodtype = "Method";
        }

        str = str
            + indent2
            + methodtype + " {\n" + indent3 + "name: \"" + m.name() + "\"\n";
        if (qstrcmp(m.typeName(), "void") != 0)
            str = str + indent3 + "type: \"" + mapTypeName(m.typeName(), false) + "\"\n";

        for (int j = 0; j < m.parameterCount(); ++j) {
            str = str
                + indent3 + "Parameter { name: \""
                + m.parameterNames().at(j) + "\"; type: \"" + mapTypeName(m.parameterTypes().at(j), true) + "\";";
            if (m.parameterTypes().at(j).endsWith('*'))
                str += " isPointer: true;";
            str = str + " }\n";
        }

        str = str + indent2 + "}\n";
    }

    str.append(indent1);
    str.append("}\n");
    return str;
}

int main(int argc, char **argv)
{
    QCoreApplication::setApplicationName(qSL("ApplicationManager qmltypes dumper"));
    QCoreApplication::setOrganizationName(qSL("Pelagicore AG"));
    QCoreApplication::setOrganizationDomain(qSL("pelagicore.com"));
    QCoreApplication::setApplicationVersion(qSL(AM_VERSION));

    QCoreApplication a(argc, argv);
    Q_UNUSED(a)
    QCommandLineParser clp;

    const char *desc =
        "Pelagicore ApplicationManager qmltypes dumper"
        "\n\n"
        "This tool is used to generate the qmltypes type information for all ApplicationManager\n"
        "classes to get auto-completion in QtCreator.\n"
        "Either install the resulting output as $QTDIR/qml/QtApplicationManager/plugins.qmltypes\n"
        "or simply run this tool with the --install option.\n";

    clp.setApplicationDescription(qL1S(desc));
    clp.addHelpOption();
    clp.addVersionOption();
    clp.addOption({ qSL("install"), qSL("directly install into the Qt directory.") });

    if (!clp.parse(QCoreApplication::arguments())) {
        fprintf(stderr, "%s\n", qPrintable(clp.errorText()));
        return 1;
    }
    if (clp.isSet(qSL("version")))
        clp.showVersion();
    if (clp.isSet(qSL("help")))
        clp.showHelp();

    QFile outFile;

    if (clp.isSet(qSL("install"))) {
        QDir qmlDir = QLibraryInfo::location(QLibraryInfo::Qml2ImportsPath);
        if (!qmlDir.exists()) {
            fprintf(stderr, "Qt's QML2 imports directory (%s) is missing.\n",
                    qPrintable(qmlDir.absolutePath()));
            return 2;
        }

        outFile.setFileName(qmlDir.absoluteFilePath(qSL("QtApplicationManager/plugins.qmltypes")));
        if (!outFile.open(QFile::WriteOnly)) {
            fprintf(stderr, "Failed to open %s for writing: %s\n", qPrintable(outFile.fileName()),
                    qPrintable(outFile.errorString()));
            return 3;
        }
    } else {
        outFile.open(stdout, QFile::WriteOnly);
    }

    QTextStream out(&outFile);

    const char *header = \
        "import QtQuick.tooling 1.2\n"
        "\n"
        "// This file describes the plugin-supplied types contained in the application-manager.\n"
        "// It is used for QML tooling purposes only.\n"
        "//\n"
        "// This file was auto-generated by:\n"
        "// appman-dumpqmltypes\n"
        "\n"
        "Module {\n"
        "    dependencies: []\n";
    const char *footer = "}\n";

    out << header;

    QVector<const QMetaObject *> all;
    all << &ApplicationManager::staticMetaObject
        << &ApplicationInstaller::staticMetaObject
        << &WindowManager::staticMetaObject
        << &NotificationManager::staticMetaObject
        << &ApplicationIPCManager::staticMetaObject
        << &Application::staticMetaObject
        << &AbstractRuntime::staticMetaObject
        << &AbstractContainer::staticMetaObject
        << &Notification::staticMetaObject
        << &QmlApplicationInterface::staticMetaObject
        << &QmlApplicationInterfaceExtension::staticMetaObject
        << &ApplicationManagerWindow::staticMetaObject;

    for (const auto &mo : qAsConst(all))
        out << qmlTypeForMetaObect(mo, 1, true);

    out << footer;
    return 0;
}
