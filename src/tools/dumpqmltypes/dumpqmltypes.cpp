/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Luxoft Application Manager.
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
#include <QtAppManManager/applicationmodel.h>
#include <QtAppManManager/amnamespace.h>
#include <QtAppManManager/applicationipcmanager.h>
#include <QtAppManManager/applicationipcinterface.h>
#include <QtAppManManager/application.h>
#include <QtAppManManager/abstractruntime.h>
#include <QtAppManManager/abstractcontainer.h>
#include <QtAppManManager/notificationmanager.h>
#include <QtAppManNotification/notification.h>
#include <QtAppManManager/notificationmanager.h>
#include <QtAppManManager/qmlinprocessapplicationinterface.h>
#include <QtAppManWindow/windowmanager.h>
#include <QtAppManWindow/window.h>
#include <QtAppManWindow/windowitem.h>
#include <QtAppManLauncher/qmlapplicationinterface.h>
#include <QtAppManLauncher/qmlapplicationinterfaceextension.h>
#include <QtAppManLauncher/private/applicationmanagerwindow_p.h>
#include <QtAppManIntentServer/intent.h>
#include <QtAppManIntentServer/intentserver.h>
#include <QtAppManIntentClient/intentclient.h>
#include <QtAppManIntentClient/intentclientrequest.h>
#include <QtAppManIntentClient/intenthandler.h>
#include <QtAppManMonitor/cpustatus.h>
#include <QtAppManMonitor/gpustatus.h>
#include <QtAppManMonitor/memorystatus.h>
#include <QtAppManMonitor/iostatus.h>
#include <QtAppManMonitor/processstatus.h>
#include <QtAppManMonitor/frametimer.h>
#include <QtAppManMonitor/monitormodel.h>
#include <QtAppManCommon/global.h>
#include <QtAppManCommon/exception.h>

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <stdio.h>

QT_USE_NAMESPACE_AM

static const QVector<const QMetaObject *> all = {
    // manager-lib
    &ApplicationManager::staticMetaObject,
    &ApplicationInstaller::staticMetaObject,
    &NotificationManager::staticMetaObject,
    &ApplicationIPCManager::staticMetaObject,
    &AbstractApplication::staticMetaObject,
    &AbstractRuntime::staticMetaObject,
    &AbstractContainer::staticMetaObject,
    &Notification::staticMetaObject,
    &QmlApplicationInterface::staticMetaObject,
    &QmlApplicationInterfaceExtension::staticMetaObject,
    &ApplicationManagerWindow::staticMetaObject,
    &ApplicationModel::staticMetaObject,
    &Am::staticMetaObject,
    &ApplicationIPCInterface::staticMetaObject,
    &ApplicationIPCInterfaceAttached::staticMetaObject,

    // window-lib
    &WindowManager::staticMetaObject,
    &Window::staticMetaObject,
    &WindowItem::staticMetaObject,

    // intent-client-lib
    &IntentClient::staticMetaObject,
    &IntentClientRequest::staticMetaObject,
    &IntentHandler::staticMetaObject,

    // intent-server-lib
    &IntentServer::staticMetaObject,
    &Intent::staticMetaObject,

    // monitor-lib
    &CpuStatus::staticMetaObject,
    &GpuStatus::staticMetaObject,
    &MemoryStatus::staticMetaObject,
    &IoStatus::staticMetaObject,
    &ProcessStatus::staticMetaObject,
    &FrameTimer::staticMetaObject,
    &MonitorModel::staticMetaObject
};


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

    // parse the AM-QmlType Q_CLASSINFO
    QByteArray type;
    int revMajor = -1;
    int revMinor = -1;
    bool isUncreatable = false;
    bool isSingleton = false;

    QMetaClassInfo c = mo->classInfo(mo->indexOfClassInfo("AM-QmlType"));
    QList<QByteArray> qmlType = QByteArray(c.value()).split(' ');
    if (qmlType.size() >= 2) {
        type = qmlType.at(0);
        QList<QByteArray> rev = qmlType.at(1).split('.');
        if (rev.size() == 2) {
            bool ok;
            revMajor = rev.at(0).toInt(&ok);
            if (!ok)
                revMajor = -1;
            revMinor = rev.at(1).toInt(&ok);
            if (!ok)
                revMinor = -1;
        }
        for (int j = 2; j < qmlType.size(); ++j) {
            const QByteArray &tag = qmlType.at(j);
            if (tag == "UNCREATABLE") {
                isUncreatable = true;
            } else if (tag == "SINGLETON") {
                isSingleton = true;
            } else {
                throw Exception("Unknown tag %1 found in AM-QmlType class info in class %2")
                        .arg(tag).arg(mo->className());
            }
        }
    }
    if (type.isEmpty() || revMajor < 0 || revMinor < 0)
        throw Exception("Class %1 has an invalid AM-QmlType class info").arg(mo->className());

    QByteArray str;
    QByteArray indent1 = QByteArray(level * 4, ' ');
    QByteArray indent2 = QByteArray((level + 1) * 4, ' ');
    QByteArray indent3 = QByteArray((level + 2) * 4, ' ');

    if (indentFirstLine)
        str.append(indent1);

    str = str
            + "Component {\n"
            + indent2 + "name: \"" + stripNamespace(mo->className()) + "\"\n"
            + indent2 + "exports: [ \"" + type + " "
                      + QByteArray::number(revMajor) + "." + QByteArray::number(revMinor) + "\" ]\n"
            + indent2 + "exportMetaObjectRevisions: [ 0 ]\n";
    if (mo->superClass())
        str = str + indent2 + "prototype: \"" + stripNamespace(mo->superClass()->className()) + "\"\n";
    if (isSingleton)
        str = str + indent2 + "isSingleton: true\n";
    if (isUncreatable)
        str = str + indent2 + "isCreatable: false\n";

    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        QMetaProperty p = mo->property(i);
        if (QByteArray(p.name()).startsWith('_')) // ignore "private"
            continue;

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
        if (m.name().startsWith('_')) // ignore "private"
            continue;
        if (isupper(m.name().at(0))) // ignore D-Bus interfaces
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

        str = str + indent2 + methodtype + " {\n" + indent3 + "name: \"" + m.name() + "\"\n";
        if (qstrcmp(m.typeName(), "void") != 0)
            str = str + indent3 + "type: \"" + mapTypeName(m.typeName(), false) + "\"\n";

        for (int j = 0; j < m.parameterCount(); ++j) {
            str = str
                    + indent3 + "Parameter { name: \""
                    + m.parameterNames().at(j) + "\"; type: \""
                    + mapTypeName(m.parameterTypes().at(j), true) + "\";";
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
    try {
        QCoreApplication::setApplicationName(qSL("ApplicationManager qmltypes dumper"));
        QCoreApplication::setOrganizationName(qSL("Luxoft Sweden AB"));
        QCoreApplication::setOrganizationDomain(qSL("luxoft.com"));
        QCoreApplication::setApplicationVersion(qSL(AM_VERSION));

        QCoreApplication a(argc, argv);
        Q_UNUSED(a)
        QCommandLineParser clp;

        const char *desc =
                "Luxoft ApplicationManager qmltypes dumper"
                "\n\n"
                "This tool is used to generate the qmltypes type information for all ApplicationManager\n"
                "classes to get auto-completion in QtCreator.\n"
                "Either install the resulting output as $QTDIR/qml/QtApplicationManager/plugins.qmltypes\n"
                "or simply run this tool with the --install option.\n";

        clp.setApplicationDescription(qL1S(desc));
        clp.addHelpOption();
        clp.addVersionOption();
        clp.addOption({ qSL("install"), qSL("directly install into the Qt directory.") });
        clp.addPositionalArgument(qSL("destination"), qSL("Destination directory where the qmltypes information is created."), qSL("dir"));

        if (!clp.parse(QCoreApplication::arguments()))
            throw Exception(clp.errorText());

        if (clp.isSet(qSL("version")))
            clp.showVersion();
        if (clp.isSet(qSL("help")))
            clp.showHelp();

        QDir outDir;

        if (clp.isSet(qSL("install")) && clp.positionalArguments().isEmpty()) {
            outDir = QLibraryInfo::location(QLibraryInfo::Qml2ImportsPath);
            if (!outDir.exists())
                throw Exception("Qt's QML2 imports directory (%1) is missing.")
                    .arg(outDir.absolutePath());
        } else if (clp.positionalArguments().count() == 1) {
            outDir = clp.positionalArguments().first();
            if (!outDir.mkpath(qSL(".")))
                throw Exception("Cannot create destination directory %1.").arg(outDir.absolutePath());
        } else {
            throw Exception("You need to either specify a destination directory or run with --install.");
        }


        // group all metaobjects by import namespace and sanity check the Q_CLASSINFOs
        QMultiMap<QString, const QMetaObject *> imports;
        QVector<QString> sanityCheck; // check for copy&paste errors

        for (const auto &mo : all) {
            bool foundClassInfo = false;
            for (int i = mo->classInfoOffset(); (i < mo->classInfoCount()); ++i) {
                QMetaClassInfo c = mo->classInfo(i);
                if (qstrcmp(c.name(), "AM-QmlType") == 0) {
                    if (foundClassInfo)
                        throw Exception("Class %1 has multiple AM-QmlType class infos").arg(mo->className());
                    foundClassInfo = true;

                    QString qmlType = qL1S(c.value());
                    imports.insert(qmlType.section(qL1C('/'), 0, 0), mo);

                    QString typeCheck = qmlType.section(qL1C(' '), 0, 1);
                    if (sanityCheck.contains(typeCheck)) {
                        throw Exception("Class %1 duplicates the type %2 already found in another class")
                            .arg(mo->className()).arg(typeCheck);
                    }
                    sanityCheck << typeCheck;
                }
            }
            if (!foundClassInfo)
                throw Exception("Class %1 is missing the AM-QmlType class info").arg(mo->className());
        }

        // go over the import namespaces and dump the metaobjects of each one
        for (auto it = imports.keyBegin(); it != imports.keyEnd(); ++it) {
            QString importPath = *it;
            importPath.replace(qL1C('.'), qL1C('/'));

            QDir importDir = outDir;
            if (!importDir.mkpath(importPath))
                throw Exception("Cannot create directory %1/%2").arg(importDir.absolutePath(), importPath);
            if (!importDir.cd(importPath))
                throw Exception("Cannot cd into directory %1/%2").arg(importDir.absolutePath(), importPath);

            QFile qmldirFile(importDir.absoluteFilePath(qSL("qmldir")));
            if (!qmldirFile.open(QFile::WriteOnly))
                throw Exception(qmldirFile, "Failed to open for writing");
            QTextStream qmldirOut(&qmldirFile);
            qmldirOut << "typeinfo plugins.qmltypes\n";
            qmldirFile.close();

            QFile typesFile(importDir.absoluteFilePath(qSL("plugins.qmltypes")));
            if (!typesFile.open(QFile::WriteOnly))
                throw Exception(typesFile, "Failed to open for writing");
            QTextStream typesOut(&typesFile);

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
                    "    dependencies: [ \"QtQuick.Window 2.${QT_MINOR_VERSION}\", \"QtQuick 2.${QT_MINOR_VERSION}\" ]\n";
            const char *footer = "}\n";

            typesOut << QByteArray(header).replace("${QT_MINOR_VERSION}",
                                                   QByteArray::number(QLibraryInfo::version().minorVersion()));

            auto mos = imports.values(*it);
            for (const auto &mo : qAsConst(mos))
                typesOut << qmlTypeForMetaObect(mo, 1, true);

            typesOut << footer;
        }
    } catch (const Exception &e) {
        fprintf(stderr, "ERROR: %s\n", qPrintable(e.errorString()));
        return 1;
    }
    return 0;
}
