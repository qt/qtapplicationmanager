/****************************************************************************
**
** Copyright (C) 2015 Pelagicore AG
** Contact: http://www.pelagicore.com/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Pelagicore Application Manager
** licenses may use this file in accordance with the commercial license
** agreement provided with the Software or, alternatively, in accordance
** with the terms contained in a written agreement between you and
** Pelagicore. For licensing terms and conditions, contact us at:
** http://www.pelagicore.com.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3 requirements will be
** met: http://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
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
