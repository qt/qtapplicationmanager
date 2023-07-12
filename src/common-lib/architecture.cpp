// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QFile>
#include <QString>
#include <QtEndian>

#include "architecture.h"

QT_BEGIN_NAMESPACE_AM


/* \internal
    Tries to identify a binary file by its magic number and returns an unique identifier.
    If \a fileName is not a binary file, an empty string is returned.

    The package-server tool uses these identifiers to support multiple architecture of the same
    package.

    https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
    https://en.wikipedia.org/wiki/Mach-O
    https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
*/
QString Architecture::identify(const QString &fileName)
{
    static const QByteArray peMagic { "MZ", 2 };
    static const QByteArray elfMagic { "\177ELF", 4 };
    static const QByteArray machoMagic { "\xcf\xfa\xed\xfe", 4 }; // 64bit, LE only
    static const QByteArray machoUniversalMagic { "\xca\xfe\xba\xbe", 4 }; // 64bit, LE only

    auto machoCpuAndBits = [](quint32 cpuId) -> QString {
        const int bits = (cpuId & 0x01000000u) ? 64 : 32;
        QString cpu;

        switch (cpuId & ~0x01000000u) {
        case 0x07: cpu = qSL("x86"); break;
        case 0x0c: cpu = qSL("arm"); break;
        }
        if (!cpu.isEmpty())
            return cpu + u'_' + QString::number(bits);
        return { };
    };


    QString arch;
    QFile f(fileName);
    if (f.open(QIODevice::ReadOnly)) {
        const QByteArray magic = f.peek(4);

        if (magic.startsWith(peMagic)) {
            if (f.seek(0x3c)) {
                if (const QByteArray offsetData = f.read(4); offsetData.size() == 4) {
                    auto offset = qFromLittleEndian<quint32>(offsetData.constData());
                    if ((offset > 0x3c) && (offset < 1000000) && f.seek(offset)) {
                        const QByteArray peHeader = f.read(6);
                        if (peHeader.startsWith("\x50\x45\x00\x00")) {
                            int bits = 0;
                            QString cpu;

                            switch (qFromLittleEndian<quint16>(peHeader.constData() + 4)) {
                            case 0x014c: bits = 32; cpu = qSL("x86"); break;
                            case 0x8664: bits = 64; cpu = qSL("x86"); break;
                            case 0xaa64: bits = 64; cpu = qSL("arm"); break;
                            }
                            if (!cpu.isEmpty())
                                arch = qSL("windows_") + cpu + u'_' + QString::number(bits);
                            else
                                arch = qSL("windows_unknown");
                        }
                    }
                }
            }
        } else if (magic == elfMagic) {
            if (QByteArray elfHeader = f.read(24); elfHeader.size() == 24) {
                const int bits = (elfHeader[4 /*EI_CLASS*/] == 1) ? 32 : 64;
                const int endianness = (elfHeader[5 /*EI_DATA*/] == 2) ? Q_BIG_ENDIAN : Q_LITTLE_ENDIAN;
                if ((elfHeader[6 /*EI_VERSION*/] == 1)
                        && ((elfHeader[7 /*EI_OSABI*/] == 0) || (elfHeader[7 /*EI_OSABI*/] == 3))) {
                    QString os = qSL("linux");

                    auto elfValue = [endianness, bits](const QByteArray &data, qsizetype baseOff,
                                                      qsizetype size32, qsizetype off32,
                                                      qsizetype size64, qsizetype off64) -> qint64 {
                        qint64 val = 0;
                        qsizetype size = (bits == 32) ? size32 : size64;
                        const char *ptr = data.data() + baseOff + ((bits == 32) ? off32 : off64);
                        while (size--)
                            val = (val << 8) | static_cast<quint8>(ptr[size]);

                        return (Q_BYTE_ORDER == endianness) ? val : qbswap(val);
                    };

                    // Android has an additional ".note.android.ident" section:
                    const qsizetype elfHeaderSize = (bits == 32) ? 52 : 64;
                    elfHeader.append(f.read(elfHeaderSize - elfHeader.size()));
                    if (elfHeader.size() == elfHeaderSize) {
                        const qint64 sectionStart       = elfValue(elfHeader, 0, 4, 0x20, 8, 0x28); // e_shoff
                        const qint64 sectionSize        = elfValue(elfHeader, 0, 2, 0x2e, 2, 0x3a); // e_shentsize
                        const qint64 sectionCount       = elfValue(elfHeader, 0, 2, 0x30, 2, 0x3c); // e_shnum
                        const qint64 stringSectionIndex = elfValue(elfHeader, 0, 2, 0x32, 2, 0x3e); // e_eshstrndx

                        if ((sectionSize <= 128) && (sectionCount < 100) // safe guard
                                && (stringSectionIndex < sectionCount)
                                && f.seek(sectionStart)) {
                            const QByteArray sectionHeaders = f.read(sectionSize * sectionCount);
                            if (sectionHeaders.size() == (sectionSize * sectionCount)) {
                                const qint64 stringsOffset = elfValue(sectionHeaders, sectionSize * stringSectionIndex, 4, 0x10, 8, 0x18); // sh_offset
                                const qint64 stringsSize   = elfValue(sectionHeaders, sectionSize * stringSectionIndex, 4, 0x14, 8, 0x20); // sh_size

                                f.seek(stringsOffset);
                                const QByteArray strings = f.read(stringsSize);
                                if (strings.size() == stringsSize) {
                                    for (int i = 0; i < sectionCount; ++i) {
                                        const qint64 name = elfValue(sectionHeaders, i * sectionSize, 4, 0x00, 4, 0x00); // sh_name
                                        const qint64 type = elfValue(sectionHeaders, i * sectionSize, 4, 0x04, 4, 0x04); // sh_type

                                        if ((type == 0x07 /*SHT_NOTE*/) && (name < strings.size())
                                                && QByteArray(strings.constData() + name).startsWith(".note.android")) {
                                            os = qSL("android");
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    QString cpu;

                    switch (elfValue(elfHeader, 0, 2, 0x12, 2, 0x12)) {
                    case 0x03:
                    case 0x3e: cpu = qSL("x86"); break;
                    case 0x28:
                    case 0xb7: cpu = qSL("arm"); break;
                    }
                    if (!cpu.isEmpty())
                        arch = os + u'_' + cpu + u'_' + QString::number(bits);
                    else
                        arch = os + qSL("_unknown");
                    if (endianness == Q_BIG_ENDIAN)
                        arch = arch + qSL("_be");
                }
            }
        } else if (magic == machoMagic) {
            QByteArray machoHeader = f.read(8);
            if (machoHeader.size() == 8) {
                const quint32 cpuId = qFromLittleEndian<quint32>(machoHeader.constData() + 4);
                if (const QString s = machoCpuAndBits(cpuId); !s.isEmpty())
                    arch = qSL("macos_") + s;
                else
                    arch = qSL("macos_unknown");
            }
        } else if (magic == machoUniversalMagic) {
            const QByteArray machoUniversalHeader = f.read(8);
            if (machoUniversalHeader.size() == 8) {
                quint32 count = qFromBigEndian<quint32>(machoUniversalHeader.constData() + 4);
                if ((count >= 1) && (count < 4)) {
                    QStringList cpuAndBits;

                    while (count--) {
                        const QByteArray machoUniversalEntry = f.read(20);
                        if (machoUniversalEntry.size() == 20) {
                            const quint32 cpuId = qFromBigEndian<quint32>(machoUniversalEntry.constData());
                            if (const QString s = machoCpuAndBits(cpuId); !s.isEmpty())
                                cpuAndBits << s;
                        }
                    }
                    if (!cpuAndBits.isEmpty()) {
                        cpuAndBits.sort();
                        arch = qSL("macos_universal_") + cpuAndBits.join(u'+');
                    } else {
                        arch = qSL("macos_universal_unknown");
                    }
                }
            }
        }
    }
    return arch;
}

QT_END_NAMESPACE_AM
