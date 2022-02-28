/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#include "startupinterface.h"

StartupInterface::~StartupInterface() { }


/*! \class StartupInterface
    \inmodule QtApplicationManager
    \brief An interface that allows to implement custom startup activities.

    This interface provides hooks that are called during the startup of application manager
    processes. Hence, implementers of the interface can run their custom code at certain points
    during the startup phase.

    A plugin has to implemet the pure virtual functions of the StartupInterface. The interface is
    the same for the System UI (appman), as well as for QML applications (appman-launcher-qml). The
    plugins that should be loaded have to be specified in the (am-config.yaml) configuration file.
    The following snippet shows how the application manager can be configured to load and execute
    the \c libappmanplugin.so in both the System UI and applications and additionally the
    \c libextplugin.so in the System UI only:

    \badcode
    # System UI
    plugins:
      startup: [ "path/to/libappmanplugin.so", "path/to/libextplugin.so" ]

    # Applications
    runtimes:
      qml:
        plugins:
          startup: "path/to/libappmanplugin.so"
    \endcode

    The functions are called in the following order:
    \list
    \li \l{StartupInterface::initialize}{initialize}
    \li afterRuntimeRegistration (optional)
    \li beforeQmlEngineLoad
    \li afterQmlEngineLoad
    \li beforeWindowShow
    \li afterWindowShow (optional)
    \endlist
*/

/*! \fn void StartupInterface::initialize(const QVariantMap &systemProperties)

    This method is called right after the \l{system properties}{\a systemProperties} have been parsed.
*/

/*! \fn void StartupInterface::afterRuntimeRegistration()

    This method is called, right after the runtime has been registered.
    \note It will only be called in the System UI process.
*/

/*! \fn void StartupInterface::beforeQmlEngineLoad(QQmlEngine *engine)

    This method is called, before the QML \a engine is loaded.

    \note All \c QtApplicationManager* QML namespaces are locked for new registrations via
          qmlProtectModule() after this call.
*/

/*! \fn void StartupInterface::afterQmlEngineLoad(QQmlEngine *engine)

    This method is called, after the QML \a engine has been loaded.
*/

/*! \fn void StartupInterface::beforeWindowShow(QWindow *window)

    This method is called, after the main window has been instantiated, but before it is shown. The
    \a window parameter holds a pointer to the main window.
    \note The \a window is only valid, if the root object of your QML tree is a visible item (e.g.
    a \l{Window} or \l{Item} derived class). It will be a \c nullptr, if it is a QtObject for
    instance.
*/

/*! \fn void StartupInterface::afterWindowShow(QWindow *window)

    This method is called, right after the main window is shown. The \a window parameter holds a
    pointer to the just shown main window.
    \note This function will only be called, if the root object of your QML tree is a visible item
    (e.g. a \l{Window} or \l{Item} derived class). It will not be called, if it is a QtObject for
    instance.
*/
