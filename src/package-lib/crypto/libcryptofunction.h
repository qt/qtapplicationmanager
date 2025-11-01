// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only
// Qt-Security score:critical reason:cryptography

#ifndef LIBCRYPTOFUNCTION_H
#define LIBCRYPTOFUNCTION_H

#include <QtCore/qglobal.h>
#include <QtAppManPackage/qtappmanpackageglobal.h>

QT_FORWARD_DECLARE_CLASS(QLibrary)

#if defined(_MSC_VER) && (_MSC_VER <= 1800)
namespace std {
    template <typename T> static typename std::add_rvalue_reference<T>::type declval();
}
#endif

QT_BEGIN_NAMESPACE_AM

template <typename R> class LibCryptoResult
{
public:
    LibCryptoResult(R r) : m_r(r) { }
    LibCryptoResult(const LibCryptoResult &other) : m_r(other.m_r) { }
    LibCryptoResult &operator=(const LibCryptoResult &that) { m_r = that.m_r; return *this; }
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
    static void initialize();

protected:
    LibCryptoFunctionBase(const char *symbol);

    void resolve();

    void (*m_functionPtr)() = nullptr;

private:
    static void loadLibCrypto();

    static QLibrary *s_library;
    static bool s_isMacOSLibreSSL;

    const char *m_symbol;
    bool m_tried = false;
};

template <typename F>
class LibCryptoFunction : protected LibCryptoFunctionBase
{
    template <typename Result, typename ...Args> static Result returnType(Result (*)(Args...));
    using R = decltype(returnType(std::declval<F>()));

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
        if (F f = functionPointer())
            return f(std::forward<Args>(args)...);
        return m_defaultResult.result();
    }
};

#define QT_AM_LIBCRYPTO_FUNCTION(f, typeof_f, ...) LibCryptoFunction<typeof_f> am_ ## f(#f, ##__VA_ARGS__)

QT_END_NAMESPACE_AM

#endif // LIBCRYPTOFUNCTION_H
