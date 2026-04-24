// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2019 Luxoft Sweden AB
// Copyright (C) 2018 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <mutex>

#include <QCoreApplication>

#if defined(Q_OS_LINUX)
#  include <QProcess>
#  include <QOffscreenSurface>
#  include <QOpenGLContext>
#  include <QOpenGLFunctions>
#endif

#include "logging.h"
#include "gpustatus.h"

using namespace Qt::StringLiterals;

/*!
    \qmltype GpuStatus
    \inqmlmodule QtApplicationManager
    \ingroup common-instantiatable
    \brief Provides information on the GPU status.

    GpuStatus provides information on the status of the GPU. Its property values are updated
    whenever the method update() is called.

    You can use it alongside a Timer for instance to periodically query the status of the GPU:

    \qml
    import QtQuick
    import QtApplicationManager
    ...
    GpuStatus { id: gpuStatus }
    Timer {
        interval: 500
        running: true
        repeat: true
        onTriggered: gpuStatus.update()
    }
    Text {
        property string loadPercent: Number(gpuStatus.gpuLoad * 100).toLocaleString(Qt.locale("en_US"), 'f', 1)
        text: "GPU load: " + loadPercent + "%"
    }
    \endqml

    You can also use this component as a MonitorModel data source if you want to plot its previous
    values over time:

    \qml
    import QtQuick
    import QtApplicationManager
    ...
    MonitorModel {
        GpuStatus {}
    }
    \endqml
*/

QT_BEGIN_NAMESPACE_AM

#if defined(Q_OS_LINUX)

class GpuVendor {
public:
    enum Vendor {
        Undefined = 0, // didn't try to determine the vendor yet
        Unsupported,
        Intel,
        Nvidia
    };
    static Vendor get();

private:
    static void fetch();
    static Vendor s_vendor;
};

GpuVendor::Vendor GpuVendor::s_vendor = GpuVendor::Undefined;

GpuVendor::Vendor GpuVendor::get()
{
    if (s_vendor == Undefined)
        fetch();

    return s_vendor;
}

void GpuVendor::fetch()
{
    QByteArray vendor;
#if !defined(QT_NO_OPENGL)
    auto readVendor = [&vendor](QOpenGLContext *c) {
        const GLubyte *p = c->functions()->glGetString(GL_VENDOR);
        if (p)
            vendor = QByteArrayView(p).toByteArray().toLower();
    };

    if (QOpenGLContext::currentContext()) {
        readVendor(QOpenGLContext::currentContext());
    } else {
        QOpenGLContext context;
        if (context.create()) {
            QOffscreenSurface surface;
            surface.setFormat(context.format());
            surface.create();
            context.makeCurrent(&surface);
            readVendor(&context);
            context.doneCurrent();
        }
    }
#endif
    if (vendor.contains("intel"))
        s_vendor = Intel;
    else if (vendor.contains("nvidia"))
        s_vendor = Nvidia;
    else
        s_vendor = Unsupported;
}

class GpuTool : protected QProcess
{
    Q_OBJECT
public:
    GpuTool()
        : QProcess(qApp)
    {
        if (GpuVendor::get() == GpuVendor::Intel) {
            setProgram(u"intel_gpu_top"_s);
            setArguments({ u"-o-"_s, u"-s 1000"_s });
        } else if (GpuVendor::get() == GpuVendor::Nvidia) {
            setProgram(u"nvidia-smi"_s);
            setArguments({ u"dmon"_s, u"--select"_s, u"u"_s });
        }

        connect(this, static_cast<void(QProcess::*)(QProcess::ProcessError error)>(&QProcess::errorOccurred),
                this, [this](QProcess::ProcessError error) {
            if (m_refCount)
                qCWarning(LogSystem) << "GPU monitoring tool:" << program() << "caused error:" << error;
        });

        connect(this, &QProcess::readyReadStandardOutput, this, [this]() {
            while (canReadLine()) {
                const QByteArray str = readLine();
                if (str.isEmpty() || (str.at(0) == '#'))
                    continue;

                int pos = (GpuVendor::get() == GpuVendor::Intel) ? 50 : 0;
                QVector<qreal> values;

                while (pos < str.size() && values.size() < 2) {
                    if (isspace(str.at(pos))) {
                        ++pos;
                        continue;
                    }
                    char *endPtr = nullptr;
#if defined(Q_OS_ANDROID)
                    qreal val = strtod(str.constData() + pos, &endPtr); // check missing for over-/underflow
#else
                    static locale_t cLocale = newlocale(LC_ALL_MASK, "C", nullptr);
                    qreal val = strtod_l(str.constData() + pos, &endPtr, cLocale); // check missing for over-/underflow
#endif
                    values << val;
                    pos = int(endPtr - str.constData() + 1);
                }

                switch (GpuVendor::get()) {
                case GpuVendor::Intel:
                    if (values.size() > 0)
                        m_lastValue = values.at(0) / 100;
                    break;
                case GpuVendor::Nvidia:
                    if (values.size() > 1) {
                        if (qFuzzyIsNull(values.at(0)))  // hardcoded to first gfx card
                            m_lastValue = values.at(1) / 100;
                    }
                    break;
                default:
                    m_lastValue = -1;
                    break;
                }
            }
        });
    }

    void ref()
    {
        if (m_refCount.ref() && !isRunning())
            start(QIODevice::ReadOnly);
    }

    void deref()
    {
        if (!m_refCount.deref() && isRunning()) {
            kill();
            waitForFinished();
        }
    }

    bool isRunning() const
    {
        return (state() == QProcess::Running);
    }

    qreal loadValue() const
    {
        return m_lastValue;
    }

private:
    QAtomicInteger<int> m_refCount;
    qreal m_lastValue = 0;
};

GpuTool *GpuStatus::s_gpuToolProcess = nullptr;

#endif // Q_OS_LINUX


GpuStatus::GpuStatus(QObject *parent)
    : QObject(parent)
{
    std::once_flag once;
    std::call_once(once, []() {
#if defined(Q_OS_LINUX)
        s_gpuToolProcess = new GpuTool();
        if (GpuVendor::get() == GpuVendor::Unsupported)
#endif
            qCWarning(LogSystem) << "GPU monitoring is not supported on this platform.";
    });
#if defined(Q_OS_LINUX)
    s_gpuToolProcess->ref();
#endif
}

GpuStatus::~GpuStatus()
{
#if defined(Q_OS_LINUX)
    s_gpuToolProcess->deref();
#endif
}

/*!
    \qmlproperty real GpuStatus::gpuLoad
    \readonly

    GPU utilization when update() was last called, as a value ranging from \c 0 (inclusive,
    completely idle) to \c 1 (inclusive, fully busy).

    \note This is dependent on tools from the graphics hardware vendor and might not work on
          every system.

    Currently, this only works on \e Linux with either \e Intel or \e NVIDIA chipsets, plus the
    tools from the respective vendors have to be installed:

    \table
    \header
        \li Hardware
        \li Tool
        \li Notes
    \row
        \li NVIDIA
        \li \c nvidia-smi
        \li The utilization will only be shown for the first GPU of the system, in case multiple GPUs
            are installed.
    \row
        \li Intel
        \li \c intel_gpu_top
        \li The binary has to be made set-UID root, e.g. via \c{sudo chmod +s $(which intel_gpu_top)},
            or the application manager has to be run as the \c root user.
    \endtable

    \sa update
*/
qreal GpuStatus::gpuLoad() const
{
    return m_gpuLoad;
}

/*!
    \qmlmethod void GpuStatus::update()

    Updates the gpuLoad property.

    \sa gpuLoad
*/
void GpuStatus::update()
{
#if defined(Q_OS_LINUX)
    qreal newLoad = s_gpuToolProcess ? s_gpuToolProcess->loadValue() : -1;
    if (!qFuzzyCompare(newLoad, m_gpuLoad)) {
        m_gpuLoad = newLoad;
        emit gpuLoadChanged();
    }
#endif
}

/*!
    \qmlproperty list<string> GpuStatus::roleNames
    \readonly

    Names of the roles provided by GpuStatus when used as a MonitorModel data source.

    \sa MonitorModel
*/
QStringList GpuStatus::roleNames() const
{
    return { u"gpuLoad"_s };
}

QT_END_NAMESPACE_AM

#include "gpustatus.moc"
#include "moc_gpustatus.cpp"
