// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "applicationmanagerwindowimpl.h"
#include "applicationmanagerwindow.h"

QT_BEGIN_NAMESPACE_AM

std::function<ApplicationManagerWindowImpl *(ApplicationManagerWindow *)> ApplicationManagerWindowImpl::s_factory;

ApplicationManagerWindowImpl::ApplicationManagerWindowImpl(ApplicationManagerWindow *window)
    : m_amwindow(window)
{ }

void ApplicationManagerWindowImpl::setFactory(const std::function<ApplicationManagerWindowImpl *(ApplicationManagerWindow *)> &factory)
{
    s_factory = factory;
}

ApplicationManagerWindowImpl *ApplicationManagerWindowImpl::create(ApplicationManagerWindow *window)
{
    return s_factory ? s_factory(window) : nullptr;
}

ApplicationManagerWindow *ApplicationManagerWindowImpl::amWindow()
{
    return m_amwindow;
}

QT_END_NAMESPACE_AM
