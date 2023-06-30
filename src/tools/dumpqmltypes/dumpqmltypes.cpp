// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QtAppManManager/packagemanager.h>
#include <QtAppManMain/applicationinstaller.h>
#include <QtAppManManager/applicationmanager.h>
#include <QtAppManManager/applicationmodel.h>
#include <QtAppManManager/amnamespace.h>
#include <QtAppManManager/application.h>
#include <QtAppManManager/abstractruntime.h>
#include <QtAppManManager/abstractcontainer.h>
#include <QtAppManManager/notificationmanager.h>
#include <QtAppManNotification/notification.h>
#include <QtAppManManager/notificationmanager.h>
#include <QtAppManManager/notificationmodel.h>
#include <QtAppManManager/qmlinprocessapplicationinterface.h>
#include <QtAppManWindow/windowmanager.h>
#include <QtAppManWindow/window.h>
#include <QtAppManWindow/windowitem.h>
#include <QtAppManLauncher/dbusapplicationinterface.h>
#include <QtAppManLauncher/private/applicationmanagerwindow_p.h>
#include <QtAppManIntentServer/intent.h>
#include <QtAppManIntentServer/intentserver.h>
#include <QtAppManIntentServer/intentmodel.h>
#include <QtAppManIntentClient/intentclient.h>
#include <QtAppManIntentClient/intentclientrequest.h>
#include <QtAppManIntentClient/intenthandler.h>
#include <QtAppManManager/intentaminterface.h>
#include <QtAppManSharedMain/cpustatus.h>
#include <QtAppManSharedMain/gpustatus.h>
#include <QtAppManSharedMain/memorystatus.h>
#include <QtAppManSharedMain/iostatus.h>
#include <QtAppManManager/processstatus.h>
#include <QtAppManSharedMain/frametimer.h>
#include <QtAppManSharedMain/monitormodel.h>
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
    &PackageManager::staticMetaObject,
    &NotificationManager::staticMetaObject,
    &NotificationModel::staticMetaObject,
    &Application::staticMetaObject,
    &AbstractRuntime::staticMetaObject,
    &AbstractContainer::staticMetaObject,
    &Notification::staticMetaObject,
    &DBusApplicationInterface::staticMetaObject,
    &ApplicationManagerWindow::staticMetaObject,
    &ApplicationModel::staticMetaObject,
    &Am::staticMetaObject,

    // window-lib
    &WindowManager::staticMetaObject,
    &Window::staticMetaObject,
    &WindowItem::staticMetaObject,

    // intent-client-lib
    &IntentClient::staticMetaObject,
    &IntentClientRequest::staticMetaObject,
    &AbstractIntentHandler::staticMetaObject,
    &IntentHandler::staticMetaObject,

    // intent-server-lib
    &IntentServer::staticMetaObject,
    &Intent::staticMetaObject,
    &IntentModel::staticMetaObject,
    &IntentServerHandler::staticMetaObject,

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
#define AM_EMPTY_TOKEN
        static const QByteArray qtamNamespace = QT_STRINGIFY(QT_PREPEND_NAMESPACE_AM(AM_EMPTY_TOKEN));
#undef AM_EMPTY_TOKEN

        if (identifier.startsWith(qtamNamespace))
            return identifier.mid(qtamNamespace.length());
        return identifier;
    };
    static auto mapTypeName = [](const QByteArray &type, bool stripPointer) -> QByteArray {
        QByteArray ba = type;
        if (ba.startsWith("const "))
            ba = ba.mid(6);
        ba = stripNamespace(ba);
        if (stripPointer && ba.endsWith('*'))
            ba.chop(1);
        return ba;
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

    // check the AM-QmlVirtualSuperClasses Q_CLASSINFO
    // used to parse up to <n> super classes in addition to the current class and use the
    // topmost one for naming (useful, if the actual type is a virtual base class)
    int parseSuperClasses = 0;
    int superIdx = mo->indexOfClassInfo("AM-QmlVirtualSuperClasses");
    if (superIdx >= mo->classInfoOffset()) {
        parseSuperClasses = QByteArray(mo->classInfo(superIdx).value()).toInt();
    }

    const QMetaObject *supermo = mo;
    for (int i = parseSuperClasses; i; --i)
        supermo = supermo->superClass();

    // parse the AM-QmlPrototype Q_CLASSINFO
    // used to specify a different prototype than the actual C++ base class
    // (useful, if the actual C++ base class should not be exposed to QML)
    QByteArray prototype = stripNamespace(supermo->superClass()->className());
    int protoIdx = mo->indexOfClassInfo("AM-QmlPrototype");
    if (protoIdx >= mo->classInfoOffset()) {
        prototype = mo->classInfo(protoIdx).value();
    }

    // parse the DefaultProperty Q_CLASSINFO
    QByteArray defaultProperty;
    int defaultPropertyIdx = mo->indexOfClassInfo("DefaultProperty");
    if (defaultPropertyIdx >= mo->classInfoOffset()) {
        defaultProperty = mo->classInfo(defaultPropertyIdx).value();
    }

    QByteArray str;
    QByteArray indent1 = QByteArray(level * 4, ' ');
    QByteArray indent2 = QByteArray((level + 1) * 4, ' ');
    QByteArray indent3 = QByteArray((level + 2) * 4, ' ');

    if (indentFirstLine)
        str.append(indent1);

    QByteArrayList exports;
    QByteArrayList exportRevs;
    for (int minor = revMinor; minor >= 0; --minor) {
        exports << QByteArray("\"" + type + " " + QByteArray::number(revMajor) + "."
                              + QByteArray::number(minor) + "\"");
        exportRevs << QByteArray::number(revMajor * 256 + minor);
    }

    str = str
          + "Component {\n"
          + indent2 + "name: \"" + stripNamespace(supermo->className()) + "\"\n"
          + indent2 + "exports: [ " + exports.join(", ") + " ]\n"
          + indent2 + "exportMetaObjectRevisions: [ " + exportRevs.join(", ") + " ]\n";
    if (mo->superClass())
        str = str + indent2 + "prototype: \"" + prototype + "\"\n";
    if (isSingleton)
        str = str + indent2 + "isSingleton: true\n";
    if (isUncreatable)
        str = str + indent2 + "isCreatable: false\n";
    if (!defaultProperty.isEmpty())
        str = str + indent2 + "defaultProperty: \"" + defaultProperty + "\"\n";


    int propertyOffset = supermo->propertyOffset();
    int methodOffset = supermo->methodOffset();
    int enumeratorOffset = supermo->enumeratorOffset();

    for (int i = enumeratorOffset; i < mo->enumeratorCount(); ++i) {
        QMetaEnum e = mo->enumerator(i);

        QByteArray values;
        for (int k = 0; k < e.keyCount(); ++k) {
            if (k)
                values.append(", ");
            values.append('"');
            values.append(e.key(k));
            values.append('"');
        }

        str = str
                + indent2
                + "Enum { name: \"" + e.enumName()
                + "\"; values: [ " + values + " ] }\n";
    }

    for (int i = propertyOffset; i < mo->propertyCount(); ++i) {
        QMetaProperty p = mo->property(i);
        if (QByteArray(p.name()).startsWith('_')) // ignore "private"
            continue;

        str = str
                + indent2
                + "Property { name: \"" + p.name()
                + "\"; type: \"" + mapTypeName(p.typeName(), true) + "\"";
        if (int rev = p.revision())
            str += "; revision: " + QByteArray::number(rev);
        if (QByteArray(p.typeName()).endsWith('*'))
            str += "; isPointer: true";
        if (!p.isWritable())
            str += "; isReadonly: true";
        if (p.isConstant())
            str += "; isConstant: true";
        if (p.isFinal())
            str += "; isFinal: true";
        str = str + " }\n";
    }

    for (int i = methodOffset; i < mo->methodCount(); ++i) {
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
        if (qstrcmp(m.typeName(), "void") != 0) {
            str = str + indent3 + "type: \"" + mapTypeName(m.typeName(), true) + "\"";
            if (int rev = m.revision())
                str += "; revision: " + QByteArray::number(rev);
            if (QByteArray(m.typeName()).endsWith('*'))
                str += "; isPointer: true";
            str += "\n";
        }

        for (int j = 0; j < m.parameterCount(); ++j) {
            str = str
                    + indent3 + "Parameter { name: \""
                    + m.parameterNames().at(j) + "\"; type: \""
                    + mapTypeName(m.parameterTypes().at(j), true) + "\"";
            if (m.parameterTypes().at(j).endsWith('*'))
                str += "; isPointer: true";
            if (m.parameterTypes().at(j).startsWith("const "))
                str += "; isConstant: true";
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
        QCoreApplication::setApplicationName(qSL("Qt Application Manager QML Types Dumper"));
        QCoreApplication::setOrganizationName(qSL("QtProject"));
        QCoreApplication::setOrganizationDomain(qSL("qt-project.org"));
        QCoreApplication::setApplicationVersion(qSL(AM_VERSION));

        QCoreApplication a(argc, argv);
        Q_UNUSED(a)
        QCommandLineParser clp;

        const char *desc =
                "\n\n"
                "This tool is used to generate the qmltypes type information for all ApplicationManager\n"
                "classes to get auto-completion in QtCreator.\n"
                "Either install the resulting output as $QTDIR/qml/QtApplicationManager/plugins.qmltypes\n"
                "or simply run this tool with the --install option.\n";

        clp.setApplicationDescription(qSL("\n") + QCoreApplication::applicationName() + qL1S(desc));
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
            outDir.setPath(QLibraryInfo::path(QLibraryInfo::Qml2ImportsPath));
            if (!outDir.exists())
                throw Exception("Qt's QML2 imports directory (%1) is missing.")
                    .arg(outDir.absolutePath());
        } else if (clp.positionalArguments().count() == 1) {
            outDir.setPath(clp.positionalArguments().constFirst());
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
            qmldirOut << "module " << *it << "\n";
            qmldirOut << "typeinfo plugins.qmltypes\n";
            qmldirOut << "depends QtQuick auto\n";
            if (it->startsWith(qSL("QtApplicationManager.")))
                qmldirOut << "import QtApplicationManager auto\n";
            qmldirFile.close();

            QFile typesFile(importDir.absoluteFilePath(qSL("plugins.qmltypes")));
            if (!typesFile.open(QFile::WriteOnly))
                throw Exception(typesFile, "Failed to open for writing");
            QTextStream typesOut(&typesFile);

            const char *header = \
                    "import QtQuick.tooling 1.2\n"
                    "\n"
                    "// This file describes the plugin-supplied types contained in the application manager.\n"
                    "// It is used for QML tooling purposes only.\n"
                    "//\n"
                    "// This file was auto-generated by:\n"
                    "// appman-dumpqmltypes\n"
                    "\n"
                    "Module {\n";
            const char *footer = "}\n";

            typesOut << QByteArray(header).replace("${QT_MINOR_VERSION}",
                                                   QByteArray::number(QLibraryInfo::version().minorVersion()));

            auto mos = imports.values(*it);
            for (const auto &mo : std::as_const(mos))
                typesOut << qmlTypeForMetaObect(mo, 1, true);

            typesOut << footer;
        }
    } catch (const Exception &e) {
        fprintf(stderr, "ERROR: %s\n", qPrintable(e.errorString()));
        return 1;
    }
    return 0;
}
