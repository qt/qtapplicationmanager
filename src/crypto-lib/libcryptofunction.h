// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#pragma once

#include <QtCore/qglobal.h>
#include <utility>
#include <QtAppManCommon/global.h>

QT_FORWARD_DECLARE_CLASS(QLibrary)

#if defined(_MSC_VER) && (_MSC_VER <= 1800)
namespace std {
    template <typename T> static typename std::add_rvalue_reference<T>::type declval();
}
#endif

QT_BEGIN_NAMESPACE_AM

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
    static bool initialize(bool loadOpenSsl3LegacyProvider);
    static inline bool isOpenSSL11() { return s_isOpenSSL11; }
    static inline bool isOpenSSL30() { return s_isOpenSSL30; }

protected:
    LibCryptoFunctionBase(const char *symbol);

    void resolve();

    const char *m_symbol;
    void (*m_functionPtr)() = nullptr;

private:
    static QLibrary *s_library;
    static bool s_isOpenSSL11;
    static bool s_isOpenSSL30;
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
        return reinterpret_cast<F>(m_functionPtr);
    }

    template <typename ...Args>
    R operator()(Args &&...args)
    {
        if (Q_UNLIKELY(!functionPointer()))
            return m_defaultResult.result();
        return std::forward<F>(reinterpret_cast<F>(m_functionPtr))(std::forward<Args>(args)...);
    }
};

#define QT_AM_LIBCRYPTO_FUNCTION(f, typeof_f, ...) Cryptography::LibCryptoFunction<typeof_f> am_ ## f(#f, ##__VA_ARGS__)

}

QT_END_NAMESPACE_AM
