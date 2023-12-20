// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once
#include <QtCore/qglobal.h>
#include <QtAppManCommon/qtappman_common-config.h>

#define QT_BEGIN_NAMESPACE_AM  namespace QtAM {
#define QT_END_NAMESPACE_AM    }
#define QT_USE_NAMESPACE_AM    using namespace QtAM;
#define QT_PREPEND_NAMESPACE_AM(name) QtAM::name

QT_BEGIN_NAMESPACE_AM
// make sure the namespace exists
QT_END_NAMESPACE_AM

#if !defined(AM_COMPILING_APPMAN) && !defined(AM_COMPILING_LAUNCHER) && !defined(QT_TESTCASE_BUILDDIR)
#  if defined(AM_FORCE_COMPILING_AGAINST_MODULES)
#    warning "You have forced compiling against AppMan modules outside of a (custom) appman binary or launcher build."
#  else
#    error "You are including headers from AppMan modules outside of a (custom) appman binary or launcher build. If you are aware of the implications of AppMan's modules being static libraries, you can #define AM_FORCE_COMPILING_AGAINST_MODULES"
#  endif
#endif
