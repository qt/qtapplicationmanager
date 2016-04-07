/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:GPL-QTAS$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: GPL-3.0
**
****************************************************************************/

#pragma once

#include <qglobal.h>
#include <utility>

QT_FORWARD_DECLARE_CLASS(QLibrary)

#if defined(_MSC_VER) && (_MSC_VER <= 1800)
namespace std {
    template <typename T> static typename std::add_rvalue_reference<T>::type declval();
}
#endif


namespace Cryptography {

template <typename R> class LibCryptoResult
{
public:
    LibCryptoResult(R r) : m_r(r) { }
    LibCryptoResult(const LibCryptoResult &other) { m_r = other.m_r; }
    LibCryptoResult operator=(const LibCryptoResult &that) { m_r = that.m_r; return *this; }
    ~LibCryptoResult() { }
    R result() { return m_r; }
private:
    R m_r;
};

template<> class LibCryptoResult<void>
{
public:
    LibCryptoResult() { }
    void result() { }
};

class LibCryptoFunctionBase
{
public:
    static bool initialize();

protected:
    LibCryptoFunctionBase(const char *symbol);

    void resolve();

    const char *m_symbol;
    void (*m_functionPtr)() = nullptr;

private:
    static QLibrary *s_library;
    bool m_tried = false;
};

template <typename F>
class LibCryptoFunction : protected LibCryptoFunctionBase
{
    template <typename Result, typename ...Args> static Result returnType(Result (*)(Args...));
    typedef decltype(returnType(std::declval<F>())) R;

    LibCryptoResult<R> m_defaultResult;

public:
    LibCryptoFunction(const char *symbol)
        : LibCryptoFunctionBase(symbol)
    { }

    LibCryptoFunction(const char *symbol, const LibCryptoResult<R> &defaultResult)
        : LibCryptoFunctionBase(symbol)
        , m_defaultResult(defaultResult)
    { }

    F functionPointer()
    {
        if (Q_UNLIKELY(!m_functionPtr))
            resolve();
        return (F) m_functionPtr;
    }

    template <typename ...Args>
    R operator()(Args &&...args)
    {
        if (Q_UNLIKELY(!functionPointer()))
            return m_defaultResult.result();
        return std::forward<F>((F) m_functionPtr)(std::forward<Args>(args)...);
    }
};

#define AM_LIBCRYPTO_FUNCTION(f, ...) Cryptography::LibCryptoFunction<decltype(&f)> am_ ## f(#f, ##__VA_ARGS__)

}
