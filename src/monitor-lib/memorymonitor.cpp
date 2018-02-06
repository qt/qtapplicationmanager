/****************************************************************************
**
** Copyright (C) 2018 Pelagicore AG
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Pelagicore Application Manager.
**
** $QT_BEGIN_LICENSE:LGPL-QTAS$
** Commercial License Usage
** Licensees holding valid commercial Qt Automotive Suite licenses may use
** this file in accordance with the commercial license agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and The Qt Company.  For
** licensing terms and conditions see https://www.qt.io/terms-conditions.
** For further information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
** SPDX-License-Identifier: LGPL-3.0
**
****************************************************************************/

#include <QDebug>
#include <QFile>
#include <QVector>
#include "global.h"
#include "logging.h"
#include "memorymonitor.h"

#if defined(Q_OS_OSX)
#  include <mach/mach.h>
#elif defined(Q_OS_LINUX)
#  include "sysfsreader.h"
#endif

QT_BEGIN_NAMESPACE_AM

namespace {
// Sizes are in bytes
enum Type {
    VmSize,
    Rss,
    Pss,
    HeapV,
    HeapR,
    HeapP,
    StackV,
    StackR,
    StackP,
};
}

class MemoryMonitorPrivate : public QObject
{
public:
    MemoryMonitorPrivate(MemoryMonitor *q)
        : q_ptr(q)
    { }

    MemoryMonitor *q_ptr;
    Q_DECLARE_PUBLIC(MemoryMonitor)

    struct smaps_sizes {
        quint64 vmSize = 0;
        int rss = 0;
        int pss = 0;
        int sharedV = 0;
        int sharedR = 0;
        int sharedP = 0;
        int ROV = 0;
        int ROR = 0;
        int ROP = 0;
        int heapV = 0;
        int heapR = 0;
        int heapP = 0;
        int stackV = 0;
        int stackR = 0;
        int stackP = 0;
        int threadStackV = 0;
        int threadStackR = 0;
        int threadStackP = 0;
        int codeV = 0;
        int codeR = 0;
        int codeP = 0;
    };
    QVector<smaps_sizes> smapSizes;

    QHash<int, QByteArray> m_roles;
    quint64 m_pid = -1;

    int m_reportPos = 0;
    int modelSize = 25;
    QList<QVariant> libraries;
    bool readLibraryList = false;

#if defined(Q_OS_LINUX)
    QScopedPointer<SysFsReader> s_smapsFs;
#endif

    const smaps_sizes &smapsForRow(int row) const
    {
        // convert a visual row position to an index into the internal ringbuffer

        int pos = row + m_reportPos;
        if (pos >= smapSizes.size())
            pos -= smapSizes.size();

        if (pos < 0 || pos >= smapSizes.size())
            return smapSizes.first();
        return smapSizes.at(pos);
    }

    void updatePid(quint64 pid)
    {
        if (m_pid == pid)
            return;
        m_pid = pid;
#if defined(Q_OS_LINUX)

        QByteArray filePath = "/proc/";
        filePath.append(QByteArray::number(m_pid));
        filePath.append("/smaps");

        // Tricky part. smaps has dynamic size
        s_smapsFs.reset(new SysFsReader(filePath.constData(), 614400));
        if (!s_smapsFs->isOpen())
            qCWarning(LogSystem) << "WARNING: could not read CPU statistics from" << s_smapsFs->fileName();

#endif
    }

    void updateModel()
    {
        Q_Q(MemoryMonitor);

        q->beginResetModel();
        // we need at least 2 items, otherwise we cannot move rows
        smapSizes.resize(modelSize);
        q->endResetModel();
    }

    void readData()
    {
        Q_Q(MemoryMonitor);
        QVector<int> roles;
        smaps_sizes t;
#if defined(Q_OS_LINUX)
        QByteArray str = s_smapsFs->readValue();
        QList<QByteArray> lines = str.split('\n');

        // Range as number of lines for each allocation in the memory
        int range = 19;
        QList<QByteArray> header;
        QList<QByteArray> lastHeader;

        for (int i = 0; i < lines.size()/range; i++) {
            lastHeader = header;
            int listIndex = i*range;

            // First line - header
            header = lines.at(listIndex).split(' ');

            if (i == 0)
                lastHeader = header;

            // Second line virtual size
            QList<QByteArray> vmSize = lines.at(listIndex + 1).split(' ');
            // Third line resident size
            QList<QByteArray> rss = lines.at(listIndex + 2).split(' ');
            // Fourth line proportional size
            QList<QByteArray> pss = lines.at(listIndex + 3).split(' ');

            //Avoid \x00 or reading data error
            // Header line should have memory address, flags
            // and library name
            if ((vmSize.size() < 3) || (pss.size() < 3) || (rss.size() < 3) || (header.size() < 5))
                break;

            int v = vmSize.at(vmSize.size() - 2).toInt() * 1000;
            t.vmSize = t.vmSize + v;

            int r = rss.at(rss.size() - 2).toInt() * 1000;
            t.rss = t.rss + r;

            int p = pss.at(pss.size() - 2).toInt() * 1000;
            t.pss = t.pss + p;

            QString libName = QString::fromLocal8Bit(header.at(header.size()-1));

            if (readLibraryList) {
                if (!libName.isEmpty()) {
                    QVariantMap map;
                    map[qSL("lib")] = libName;
                    map[qSL("vSize")] = v;
                    map[qSL("rSize")] = r;
                    map[qSL("pSize")] = p;
                    libraries.append(QVariant::fromValue(map));
                }
            }

            QByteArray type = header.at(header.size() - 1);
            QByteArray flags = header.at(1);
            QByteArray lastFlags = lastHeader.at(1);
            QByteArray temp = header.at(3);

            if (type == "[heap]") {
                t.heapV = t.heapV + v;
                t.heapR = t.heapR + r;
                t.heapP = t.heapP + p;
            }
            else if (type == "[stack]") {
                t.stackV = t.stackV + v;
                t.stackR = t.stackR + r;
                t.stackP = t.stackP + p;
            }
            else if (flags == "rw-p") {
                if (v == 8192 && temp == "00:00" && lastFlags == "---p") {
                    /*
                     * thread stack we skip for now
                    t.threadStackV = t.threadStackV + v;
                    t.threadStackR = t.threadStackR + r;
                    t.threadStackP = t.threadStackP + p;
                    */

                    t.stackV = t.stackV + v;
                    t.stackR = t.stackR + r;
                    t.stackP = t.stackP + p;
                }
                else if (temp == "00:00") {
                    t.heapV = t.heapV + v;
                    t.heapR = t.heapR + r;
                    t.heapP = t.heapP + p;
                }
                /*
                 * Can be added by the need
                else {
                    t.sharedV = t.sharedV + v;
                    t.sharedR = t.sharedR + r;
                    t.sharedP = t.sharedP + p;
                }
                */
            }

            /*
             * Can be added by the need
            else if (flags == "rw-s" && temp != "00:00") {
                t.sharedV = t.sharedV + v;
                t.sharedR = t.sharedR + r;
                t.sharedP = t.sharedP + p;
            }
            else if (flags == "r--p" || flags == "r--s") {
                t.ROV = t.ROV + v;
                t.ROR = t.ROR + r;
                t.ROP = t.ROP + p;
            }
            else if (flags == "r-xp") {
                t.codeV = t.codeV + v;
                t.codeR = t.codeR + r;
                t.codeP = t.codeP + p;
            }
            */

        }

        // ring buffer handling
        // optimization: instead of sending a dataChanged for every item, we always move the
        // first item to the end and change its data only
        roles.append(VmSize);
        roles.append(Rss);
        roles.append(Pss);
        roles.append(HeapV);
        roles.append(HeapR);
        roles.append(HeapP);
        roles.append(StackV);
        roles.append(StackR);
        roles.append(StackP);

        int size = smapSizes.size();
        q->beginMoveRows(QModelIndex(), 0, 0, QModelIndex(), size);
        smapSizes[m_reportPos++] = t;
        if (m_reportPos >= smapSizes.size())
            m_reportPos = 0;
        q->endMoveRows();
        q->dataChanged(q->index(size - 1), q->index(size - 1), roles);

        int sentIndex = size - m_reportPos;
        if (sentIndex < 0)
            sentIndex = 0;
        else if (sentIndex > (modelSize - 1))
            sentIndex = modelSize - 1;

        emit q->memoryReportingChanged(sentIndex);

#elif defined(Q_OS_OSX)

        struct task_basic_info t_info;
        mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

        if (KERN_SUCCESS != task_info(mach_task_self(),
                                      TASK_BASIC_INFO, (task_info_t)&t_info,
                                      &t_info_count))
        {
            qCWarning(LogSystem) << "WARNING Could not read the memory data";
            return;
        }

        roles.append(VmSize);
        roles.append(Rss);
        roles.append(Pss);
        roles.append(HeapV);
        roles.append(HeapR);
        roles.append(HeapP);
        roles.append(StackV);
        roles.append(StackR);
        roles.append(StackP);

        t.rss = t_info.resident_size;
        t.vmSize = t_info.virtual_size;

        int size = smapSizes.size();
        q->beginMoveRows(QModelIndex(), 0, 0, QModelIndex(), size);
        smapSizes[m_reportPos++] = t;
        if (m_reportPos >= smapSizes.size())
            m_reportPos = 0;
        q->endMoveRows();
        q->dataChanged(q->index(size - 1), q->index(size - 1), roles);

        int sentIndex = size - m_reportPos;
        if (sentIndex < 0)
            sentIndex = 0;
        else if (sentIndex > (modelSize - 1))
            sentIndex = modelSize - 1;

        emit q->memoryReportingChanged(sentIndex);

#else
    Q_UNUSED(q)
#endif
    }

};

MemoryMonitor::MemoryMonitor()
    : d_ptr(new MemoryMonitorPrivate(this))
{
    Q_D(MemoryMonitor);

    d->m_roles[VmSize] = "vmSize";
    d->m_roles[Rss] = "rss";
    d->m_roles[Pss] = "pss";
    d->m_roles[HeapV] = "heapV";
    d->m_roles[HeapR] = "heapR";
    d->m_roles[HeapP] = "heapP";
    d->m_roles[StackV] = "stackV";
    d->m_roles[StackR] = "stackR";
    d->m_roles[StackP] = "stackP";


    d->updateModel();
}

MemoryMonitor::~MemoryMonitor()
{
    Q_D(MemoryMonitor);
    delete d;
}

int MemoryMonitor::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    Q_D(const MemoryMonitor);
    return d->smapSizes.size();
}

int MemoryMonitor::count() const
{
    Q_D(const MemoryMonitor);
    return d->smapSizes.size();
}

QVariant MemoryMonitor::data(const QModelIndex &index, int role) const
{
    Q_D(const MemoryMonitor);
    if (!index.isValid() || index.row() < 0 || index.row() >= d->smapSizes.size()) {
        return QVariant();
    }

    const MemoryMonitorPrivate::smaps_sizes &t = d->smapsForRow(index.row());

    switch (role) {
    case VmSize:
        return t.vmSize;
    case Rss:
        return t.rss;
    case Pss:
        return t.pss;
    case HeapV:
        return t.heapV;
    case HeapR:
        return t.heapR;
    case HeapP:
        return t.heapP;
    case StackV:
        return t.stackV;
    case StackR:
        return t.stackR;
    case StackP:
        return t.stackP;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MemoryMonitor::roleNames() const
{
    Q_D(const MemoryMonitor);
    return d->m_roles;
}

QVariantMap MemoryMonitor::get(int row) const
{
    if (row < 0 || row >= count()) {
        qDebug() << Q_FUNC_INFO <<"Invalid row:" << row << "count:" << rowCount();
        return QVariantMap();
    }

    QVariantMap map;
    QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        map.insert(qL1S(it.value()), data(index(row), it.key()));
    }

    return map;
}

void MemoryMonitor::readData()
{
    Q_D(MemoryMonitor);
    d->readData();
}

void MemoryMonitor::setPid(quint64 pid)
{
    Q_D(MemoryMonitor);
    d->updatePid(pid);
}

QList<QVariant> MemoryMonitor::getLibraryList()
{
    Q_D(MemoryMonitor);
    d->libraries.clear();
    d->readLibraryList = true;
    d->readData();
    d->readLibraryList = false;
    return d->libraries;
}

QT_END_NAMESPACE_AM
