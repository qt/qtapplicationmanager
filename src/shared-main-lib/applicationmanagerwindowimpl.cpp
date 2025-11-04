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


///////////////////////////////////////////////////////////////////////////////////////////////////
// ApplicationManagerWindowAttachedImpl
///////////////////////////////////////////////////////////////////////////////////////////////////


std::function<ApplicationManagerWindowAttachedImpl *(ApplicationManagerWindowAttached *, QQuickItem *)> ApplicationManagerWindowAttachedImpl::s_factory;

ApplicationManagerWindowAttachedImpl::ApplicationManagerWindowAttachedImpl(ApplicationManagerWindowAttached *windowAttached,
                                                                           QQuickItem *attacheeItem)
    : m_amWindowAttached(windowAttached)
    , m_attacheeItem(attacheeItem)
{ }

void ApplicationManagerWindowAttachedImpl::setFactory(const std::function<ApplicationManagerWindowAttachedImpl *(ApplicationManagerWindowAttached *, QQuickItem *)> &factory)
{
    s_factory = factory;
}

ApplicationManagerWindowAttachedImpl *ApplicationManagerWindowAttachedImpl::create(ApplicationManagerWindowAttached *windowAttached, QQuickItem *attacheeItem)
{
    return s_factory ? s_factory(windowAttached, attacheeItem) : nullptr;
}

ApplicationManagerWindowAttached *ApplicationManagerWindowAttachedImpl::amWindowAttached() const
{
    return m_amWindowAttached;
}

QQuickItem *ApplicationManagerWindowAttachedImpl::attacheeItem() const
{
    return m_attacheeItem;
}

void ApplicationManagerWindowAttachedImpl::onWindowChanged(ApplicationManagerWindow *newWin)
{
    m_amWindowAttached->reconnect(newWin);
}


QT_END_NAMESPACE_AM
