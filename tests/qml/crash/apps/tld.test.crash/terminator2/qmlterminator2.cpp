/****************************************************************************
**
** Copyright (C) 2019 Luxoft Sweden AB
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Application Manager.
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

#include <QQmlEngine>
#include <QJSEngine>

#include "qmlterminator2.h"

#include <signal.h>
#include <stdlib.h>


static QObject *terminator_provider(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(scriptEngine)
    return new Terminator(engine);
}

void TerminatorPlugin::registerTypes(const char *uri)
{
    qmlRegisterSingletonType<Terminator>(uri, 2, 0, "Terminator", terminator_provider);
}


static void abortWithVeryLongSymbolNameOnTheStack800CharactersLong_CallMeIshmaelSomeYearsAgoNeverMindHowLongPreciselyHavingLittleOrNoMoneyInMyPurseAndNothingParticularToInterestMeOnShoreIThoughIWouldSailAboutALittlAndSeeTheWateryPartOfTheWorldItIsAWayIHaveOfDrivingOffTheSpleenAndRegulatingTheCirculationWhenenverIFindMyselfGrowingGrimAboutTheMouthWheneverItIsADampDrizzlyNovemberInMySoulWheneverIFindMyselfInvoluntarilyPausingBeforeCoffinWarehousesAndBringingUpTheRearOfEveryFuneralIMeetAndEspeciallyWheneverMyHyposGetSuchAnUpperHandOfMeThatItRequiresAStrongMoralPrincipleToPreventMeFromDeliberatelySteppingIntoTheStreetAndMethodicallyKnockingPeoplesHatsOffThenIAccountItHighTimeToGetToSeaAsSoonAsICanThisIsMySubstituteForPistolAndBallWithAPhilosophicalFlourishCatoThrowsHimselfUponHisSwordIQuietlyTakeToTheShip()
{
    ::abort();
}

void Terminator::accessIllegalMemory() const
{
    *(int*)1 = 42;
}

void Terminator::accessIllegalMemoryInThread()
{
    TerminatorThread *t = new TerminatorThread(this);
    t->start();
}

void Terminator::forceStackOverflow() const
{
    static constexpr int len = 100000;
    volatile char buf[len];
    buf[len-1] = 42;
    if (buf[len-1] == 42)
        forceStackOverflow();
}

void Terminator::divideByZero() const
{
    int d = 0;
    volatile int x = 42 / d;
    Q_UNUSED(x)
}

void Terminator::abort() const
{
    abortWithVeryLongSymbolNameOnTheStack800CharactersLong_CallMeIshmaelSomeYearsAgoNeverMindHowLongPreciselyHavingLittleOrNoMoneyInMyPurseAndNothingParticularToInterestMeOnShoreIThoughIWouldSailAboutALittlAndSeeTheWateryPartOfTheWorldItIsAWayIHaveOfDrivingOffTheSpleenAndRegulatingTheCirculationWhenenverIFindMyselfGrowingGrimAboutTheMouthWheneverItIsADampDrizzlyNovemberInMySoulWheneverIFindMyselfInvoluntarilyPausingBeforeCoffinWarehousesAndBringingUpTheRearOfEveryFuneralIMeetAndEspeciallyWheneverMyHyposGetSuchAnUpperHandOfMeThatItRequiresAStrongMoralPrincipleToPreventMeFromDeliberatelySteppingIntoTheStreetAndMethodicallyKnockingPeoplesHatsOffThenIAccountItHighTimeToGetToSeaAsSoonAsICanThisIsMySubstituteForPistolAndBallWithAPhilosophicalFlourishCatoThrowsHimselfUponHisSwordIQuietlyTakeToTheShip();
}

void Terminator::raise(int sig) const
{
    ::raise(sig);
}

void Terminator::throwUnhandledException() const
{
    throw 42;
}

void Terminator::exitGracefully() const
{
    exit(5);
}


TerminatorThread::TerminatorThread(Terminator *parent)
    : QThread(parent), m_terminator(parent)
{
}

void TerminatorThread::run()
{
    m_terminator->accessIllegalMemory();
}

#include "moc_qmlterminator2.cpp"
