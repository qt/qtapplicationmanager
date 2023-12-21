// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <QQmlEngine>
#include <QJSEngine>

#include "qmlcrash.h"

#include <signal.h>
#include <stdlib.h>


static QObject *qmlCrash_provider(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(scriptEngine)
    return new QmlCrash(engine);
}

void QmlCrashPlugin::registerTypes(const char *uri)
{
    qmlRegisterSingletonType<QmlCrash>(uri, 2, 0, "QmlCrash", qmlCrash_provider);
}


[[noreturn]] static void abortWithVeryLongSymbolNameOnTheStack800CharactersLong_CallMeIshmaelSomeYearsAgoNeverMindHowLongPreciselyHavingLittleOrNoMoneyInMyPurseAndNothingParticularToInterestMeOnShoreIThoughIWouldSailAboutALittlAndSeeTheWateryPartOfTheWorldItIsAWayIHaveOfDrivingOffTheSpleenAndRegulatingTheCirculationWhenenverIFindMyselfGrowingGrimAboutTheMouthWheneverItIsADampDrizzlyNovemberInMySoulWheneverIFindMyselfInvoluntarilyPausingBeforeCoffinWarehousesAndBringingUpTheRearOfEveryFuneralIMeetAndEspeciallyWheneverMyHyposGetSuchAnUpperHandOfMeThatItRequiresAStrongMoralPrincipleToPreventMeFromDeliberatelySteppingIntoTheStreetAndMethodicallyKnockingPeoplesHatsOffThenIAccountItHighTimeToGetToSeaAsSoonAsICanThisIsMySubstituteForPistolAndBallWithAPhilosophicalFlourishCatoThrowsHimselfUponHisSwordIQuietlyTakeToTheShip()
{
    ::abort();
}

void QmlCrash::accessIllegalMemory() const
{
    *reinterpret_cast<int *>(1) = 42;
}

void QmlCrash::accessIllegalMemoryInThread()
{
    QmlCrashThread *t = new QmlCrashThread(this);
    t->start();
}

void QmlCrash::forceStackOverflow() const
{
    static constexpr int len = 100000;
    volatile char buf[len];
    buf[len-1] = 42;
    if (buf[len-1] == 42)
        forceStackOverflow();
}

void QmlCrash::divideByZero() const
{
    int d = 0;
    volatile int x = 42 / d;
    Q_UNUSED(x)
}

void QmlCrash::abort() const
{
    abortWithVeryLongSymbolNameOnTheStack800CharactersLong_CallMeIshmaelSomeYearsAgoNeverMindHowLongPreciselyHavingLittleOrNoMoneyInMyPurseAndNothingParticularToInterestMeOnShoreIThoughIWouldSailAboutALittlAndSeeTheWateryPartOfTheWorldItIsAWayIHaveOfDrivingOffTheSpleenAndRegulatingTheCirculationWhenenverIFindMyselfGrowingGrimAboutTheMouthWheneverItIsADampDrizzlyNovemberInMySoulWheneverIFindMyselfInvoluntarilyPausingBeforeCoffinWarehousesAndBringingUpTheRearOfEveryFuneralIMeetAndEspeciallyWheneverMyHyposGetSuchAnUpperHandOfMeThatItRequiresAStrongMoralPrincipleToPreventMeFromDeliberatelySteppingIntoTheStreetAndMethodicallyKnockingPeoplesHatsOffThenIAccountItHighTimeToGetToSeaAsSoonAsICanThisIsMySubstituteForPistolAndBallWithAPhilosophicalFlourishCatoThrowsHimselfUponHisSwordIQuietlyTakeToTheShip();
}

void QmlCrash::raise(int sig) const
{
    ::raise(sig);
}

void QmlCrash::throwUnhandledException() const
{
    throw 42;
}

void QmlCrash::exitGracefully() const
{
    exit(5);
}


QmlCrashThread::QmlCrashThread(QmlCrash *parent)
    : QThread(parent)
    , m_qmlCrash(parent)
{ }

void QmlCrashThread::run()
{
    m_qmlCrash->accessIllegalMemory();
}

#include "moc_qmlcrash.cpp"
