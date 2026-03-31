#include "BecTool.h"
#include "GladiusHashes.h"
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QTextStream>
#include <QFileInfo>
#include <zlib.h>
#include <algorithm>
#include <future>
#include <numeric>
#include <vector>
#include <thread>
#include <filesystem>

namespace Tools {

// ── Constants ─────────────────────────────────────────────────────────────────

static constexpr quint32 kDuplicateFlag = 0x2000000u;
static constexpr quint32 kFileAtEnd     = 0xC0000000u;
static constexpr int     kChecksumSize  = 8;
static const char *kHeaderFile  = "filelist.txt";
static const char *kMagic       = " ceb";

// ── CRC32 table (from bec-tool-all.py) ────────────────────────────────────────

static const quint32 kCRCTable[256] = {
    0x00000000,0x77073096,0xee0e612c,0x990951ba,0x076dc419,0x706af48f,0xe963a535,0x9e6495a3,
    0x0edb8832,0x79dcb8a4,0xe0d5e91e,0x97d2d988,0x09b64c2b,0x7eb17cbd,0xe7b82d07,0x90bf1d91,
    0x1db71064,0x6ab020f2,0xf3b97148,0x84be41de,0x1adad47d,0x6ddde4eb,0xf4d4b551,0x83d385c7,
    0x136c9856,0x646ba8c0,0xfd62f97a,0x8a65c9ec,0x14015c4f,0x63066cd9,0xfa0f3d63,0x8d080df5,
    0x3b6e20c8,0x4c69105e,0xd56041e4,0xa2677172,0x3c03e4d1,0x4b04d447,0xd20d85fd,0xa50ab56b,
    0x35b5a8fa,0x42b2986c,0xdbbbc9d6,0xacbcf940,0x32d86ce3,0x45df5c75,0xdcd60dcf,0xabd13d59,
    0x26d930ac,0x51de003a,0xc8d75180,0xbfd06116,0x21b4f4b5,0x56b3c423,0xcfba9599,0xb8bda50f,
    0x2802b89e,0x5f058808,0xc60cd9b2,0xb10be924,0x2f6f7c87,0x58684c11,0xc1611dab,0xb6662d3d,
    0x76dc4190,0x01db7106,0x98d220bc,0xefd5102a,0x71b18589,0x06b6b51f,0x9fbfe4a5,0xe8b8d433,
    0x7807c9a2,0x0f00f934,0x9609a88e,0xe10e9818,0x7f6a0dbb,0x086d3d2d,0x91646c97,0xe6635c01,
    0x6b6b51f4,0x1c6c6162,0x856530d8,0xf262004e,0x6c0695ed,0x1b01a57b,0x8208f4c1,0xf50fc457,
    0x65b0d9c6,0x12b7e950,0x8bbeb8ea,0xfcb9887c,0x62dd1ddf,0x15da2d49,0x8cd37cf3,0xfbd44c65,
    0x4db26158,0x3ab551ce,0xa3bc0074,0xd4bb30e2,0x4adfa541,0x3dd895d7,0xa4d1c46d,0xd3d6f4fb,
    0x4369e96a,0x346ed9fc,0xad678846,0xda60b8d0,0x44042d73,0x33031de5,0xaa0a4c5f,0xdd0d7cc9,
    0x5005713c,0x270241aa,0xbe0b1010,0xc90c2086,0x5768b525,0x206f85b3,0xb966d409,0xce61e49f,
    0x5edef90e,0x29d9c998,0xb0d09822,0xc7d7a8b4,0x59b33d17,0x2eb40d81,0xb7bd5c3b,0xc0ba6cad,
    0xedb88320,0x9abfb3b6,0x03b6e20c,0x74b1d29a,0xead54739,0x9dd277af,0x04db2615,0x73dc1683,
    0xe3630b12,0x94643b84,0x0d6d6a3e,0x7a6a5aa8,0xe40ecf0b,0x9309ff9d,0x0a00ae27,0x7d079eb1,
    0xf00f9344,0x8708a3d2,0x1e01f268,0x6906c2fe,0xf762575d,0x806567cb,0x196c3671,0x6e6b06e7,
    0xfed41b76,0x89d32be0,0x10da7a5a,0x67dd4acc,0xf9b9df6f,0x8ebeeff9,0x17b7be43,0x60b08ed5,
    0xd6d6a3e8,0xa1d1937e,0x38d8c2c4,0x4fdff252,0xd1bb67f1,0xa6bc5767,0x3fb506dd,0x48b2364b,
    0xd80d2bda,0xaf0a1b4c,0x36034af6,0x41047a60,0xdf60efc3,0xa867df55,0x316e8eef,0x4669be79,
    0xcb61b38c,0xbc66831a,0x256fd2a0,0x5268e236,0xcc0c7795,0xbb0b4703,0x220216b9,0x5505262f,
    0xc5ba3bbe,0xb2bd0b28,0x2bb45a92,0x5cb36a04,0xc2d7ffa7,0xb5d0cf31,0x2cd99e8b,0x5bdeae1d,
    0x9b64c2b0,0xec63f226,0x756aa39c,0x026d930a,0x9c0906a9,0xeb0e363f,0x72076785,0x05005713,
    0x95bf4a82,0xe2b87a14,0x7bb12bae,0x0cb61b38,0x92d28e9b,0xe5d5be0d,0x7cdcefb7,0x0bdbdf21,
    0x86d3d2d4,0xf1d4e242,0x68ddb3f8,0x1fda836e,0x81be16cd,0xf6b9265b,0x6fb077e1,0x18b74777,
    0x88085ae6,0xff0f6a70,0x66063bca,0x11010b5c,0x8f659eff,0xf862ae69,0x616bffd3,0x166ccf45,
    0xa00ae278,0xd70dd2ee,0x4e048354,0x3903b3c2,0xa7672661,0xd06016f7,0x4969474d,0x3e6e77db,
    0xaed16a4a,0xd9d65adc,0x40df0b66,0x37d83bf0,0xa9bcae53,0xdebb9ec5,0x47b2cf7f,0x30b5ffe9,
    0xbdbdf21c,0xcabac28a,0x53b39330,0x24b4a3a6,0xbad03605,0xcdd70693,0x54de5729,0x23d967bf,
    0xb3667a2e,0xc4614ab8,0x5d681b02,0x2a6f2b94,0xb40bbe37,0xc30c8ea1,0x5a05df1b,0x2d02ef8d,
};

static quint32 computeFileHash(const QString &name)
{
    QByteArray bytes = name.toLatin1();
    quint32 hashVal = 0;
    for (char ch : bytes) {
        quint32 key = (hashVal ^ static_cast<quint8>(ch)) & 0xff;
        hashVal = kCRCTable[key] ^ (hashVal >> 8);
    }
    return hashVal;
}

// ── zlib helpers ──────────────────────────────────────────────────────────────

static QByteArray zlibDecompress(const QByteArray &compressed)
{
    z_stream strm{};
    if (inflateInit(&strm) != Z_OK) return {};
    strm.avail_in = static_cast<uInt>(compressed.size());
    strm.next_in  = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.data()));
    QByteArray result;
    char buf[65536];
    int ret;
    do {
        strm.avail_out = sizeof(buf);
        strm.next_out  = reinterpret_cast<Bytef *>(buf);
        ret = inflate(&strm, Z_NO_FLUSH);
        result.append(buf, sizeof(buf) - strm.avail_out);
    } while (ret == Z_OK);
    inflateEnd(&strm);
    return result;
}

static QByteArray zlibCompress(const QByteArray &data)
{
    uLongf destLen = compressBound(static_cast<uLong>(data.size()));
    QByteArray result(static_cast<int>(destLen), '\0');
    compress2(reinterpret_cast<Bytef *>(result.data()), &destLen,
              reinterpret_cast<const Bytef *>(data.constData()),
              static_cast<uLong>(data.size()), Z_BEST_SPEED);
    result.resize(static_cast<int>(destLen));
    return result;
}

// ── RomSection ────────────────────────────────────────────────────────────────

struct RomSection {
    QString  fileName;
    quint32  pathHash       = 0;
    quint32  dataOffset     = 0;
    quint32  origDataOffset = 0;
    quint32  dataSize       = 0;
    quint32  compressedSize = 0; // 0 = not compressed
    bool     isDuplicate    = false;
    bool     isNew          = false;
    QByteArray fileData;    // loaded during pack phase 1

    quint32 unpackedCompSize() const { return compressedSize & ~kDuplicateFlag; }
    quint32 storedSize()       const { return unpackedCompSize() ? unpackedCompSize() : dataSize; }
};

static void writeU32LE(QFile &f, quint32 v)
{
    quint8 buf[4] = { quint8(v), quint8(v>>8), quint8(v>>16), quint8(v>>24) };
    f.write(reinterpret_cast<const char *>(buf), 4);
}

static quint32 readU32LE(const QByteArray &data, int off)
{
    quint8 b0 = static_cast<quint8>(data[off]);
    quint8 b1 = static_cast<quint8>(data[off+1]);
    quint8 b2 = static_cast<quint8>(data[off+2]);
    quint8 b3 = static_cast<quint8>(data[off+3]);
    return b0 | (quint32(b1)<<8) | (quint32(b2)<<16) | (quint32(b3)<<24);
}

static void alignFileWithZeros(QFile &f, quint32 alignment)
{
    quint32 pos = static_cast<quint32>(f.pos());
    quint32 target = (pos + alignment - 1) & ~(alignment - 1);
    qint64 pad = target - pos;
    if (pad > 0) f.write(QByteArray(static_cast<int>(pad), '\0'));
}

// ── UNPACK ────────────────────────────────────────────────────────────────────

bool becUnpack(const QString &becFile, const QString &outDir,
               BecPlatform /*platform*/, bool demoBec,
               const LogCb &out, const LogCb &err)
{
    GladiusHashes::load(); // best-effort; unknown files get generic names

    QFile f(becFile);
    if (!f.open(QIODevice::ReadOnly)) {
        err(QStringLiteral("Cannot open BEC: ") + becFile);
        return false;
    }

    quint32 fileAlignment = 0, nrOfFiles = 0, headerMagic = 0;
    if (demoBec) {
        f.seek(0x4);
        QByteArray h = f.read(4);
        nrOfFiles = readU32LE(h, 0);
    } else {
        f.seek(0x6);
        QByteArray h = f.read(10);
        fileAlignment = quint32(static_cast<quint8>(h[0])) | (quint32(static_cast<quint8>(h[1]))<<8);
        nrOfFiles     = readU32LE(h, 2);
        headerMagic   = readU32LE(h, 6);
    }

    out(QStringLiteral("BEC: %1 files, alignment=%2").arg(nrOfFiles).arg(fileAlignment));

    // Read entry table (16 bytes each, right after the 16-byte header)
    f.seek(0x10);
    QVector<RomSection> sections;
    sections.reserve(static_cast<int>(nrOfFiles));
    int unknownCount = 0;
    for (quint32 i = 0; i < nrOfFiles; ++i) {
        QByteArray entry = f.read(0x10);
        quint32 pathHash   = readU32LE(entry, 0);
        quint32 dataOffset = readU32LE(entry, 4);
        quint32 compSize   = readU32LE(entry, 8);
        quint32 dataSize   = readU32LE(entry, 12);

        RomSection s;
        s.pathHash        = pathHash;
        s.dataOffset      = dataOffset;
        s.origDataOffset  = dataOffset;
        s.compressedSize  = compSize;
        s.dataSize        = dataSize;
        s.isDuplicate     = (compSize & kDuplicateFlag) != 0;
        s.fileName        = GladiusHashes::lookup(pathHash, unknownCount);
        if (s.fileName.startsWith(QStringLiteral("unknown-"))) ++unknownCount;
        sections.append(s);
    }

    // Sort by data offset for sequential reading
    std::sort(sections.begin(), sections.end(), [](const RomSection &a, const RomSection &b) {
        return a.dataOffset < b.dataOffset;
    });

    // Build file list header line
    QString fileListOutput = QString::number(fileAlignment) + ','
                           + QString::number(nrOfFiles)     + ','
                           + QString::number(headerMagic)   + '\n';

    // Parallel decompress + write using std::async, batched to limit concurrency
    int numThreads = static_cast<int>(std::max(4u, std::thread::hardware_concurrency()));
    bool ok = true;
    int completed = 0;
    int total = sections.size();

    for (int batch = 0; batch < total; batch += numThreads) {
        int end = std::min(batch + numThreads, total);
        std::vector<std::future<bool>> futures;
        futures.reserve(static_cast<size_t>(end - batch));

        for (int i = batch; i < end; ++i) {
            RomSection s = sections[i]; // capture by value
            futures.push_back(std::async(std::launch::async, [s, &becFile, &outDir, &err]() -> bool {
                QFile rf(becFile);
                if (!rf.open(QIODevice::ReadOnly)) return false;
                rf.seek(s.dataOffset);
                QByteArray data = rf.read(s.dataSize);
                if (!data.isEmpty() && static_cast<quint8>(data[0]) == 0x78)
                    data = zlibDecompress(data);

                QString fullPath = outDir + s.fileName;
                std::filesystem::create_directories(
                    std::filesystem::path(fullPath.toStdString()).parent_path());
                QFile wf(fullPath);
                if (!wf.open(QIODevice::WriteOnly)) {
                    err(QStringLiteral("Cannot write: ") + fullPath);
                    return false;
                }
                wf.write(data);
                return true;
            }));
        }

        for (auto &fut : futures) {
            ok &= fut.get();
            ++completed;
            if (completed % 500 == 0)
                out(QStringLiteral("Unpack progress: %1/%2").arg(completed).arg(total));
        }
    }

    // Build filelist.txt (in original order, sorted by DataOffset)
    for (const RomSection &s : sections) {
        fileListOutput += s.fileName + ','
                        + QString::number(s.pathHash)       + ','
                        + QString::number(s.dataOffset)     + ','
                        + QString::number(s.compressedSize) + ','
                        + QString::number(s.dataSize)       + '\n';
    }

    // Write filelist.txt
    QString hdrPath = outDir + kHeaderFile;
    QDir().mkpath(QFileInfo(hdrPath).absolutePath());
    QFile hf(hdrPath);
    if (hf.open(QIODevice::WriteOnly | QIODevice::Text))
        hf.write(fileListOutput.toUtf8());

    out(QStringLiteral("BEC unpack complete: %1 files, %2 unknown").arg(sections.size()).arg(unknownCount));
    return ok;
}

// ── PACK ──────────────────────────────────────────────────────────────────────

bool becPack(const QString &inDir, const QString &outFile,
             const QString &fileListPath, BecPlatform platform,
             const LogCb &out, const LogCb &err)
{
    // Read filelist.txt
    QFile listFile(fileListPath);
    if (!listFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err(QStringLiteral("Cannot open filelist: ") + fileListPath);
        return false;
    }

    quint32 fileAlignment = 0, headerMagic = 0;
    QVector<RomSection> romMap;
    {
        QTextStream in(&listFile);
        int lineIdx = 0;
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith('#')) continue;
            QStringList parts = line.split(',');
            if (lineIdx == 0) {
                fileAlignment = parts.value(0).toUInt();
                // parts[1] = file count (we'll recompute)
                headerMagic   = parts.value(2).toUInt();
            } else if (parts.size() >= 5) {
                RomSection s;
                s.fileName        = parts[0].replace('\\', '/').toLower();
                s.pathHash        = parts[1].toUInt();
                if (s.pathHash == 0)
                    s.pathHash    = computeFileHash(s.fileName);
                s.dataOffset      = parts[2].toUInt();
                s.origDataOffset  = s.dataOffset;
                s.compressedSize  = parts[3].toUInt();
                s.dataSize        = parts[4].toUInt();
                s.isDuplicate     = (s.compressedSize & kDuplicateFlag) != 0;
                romMap.append(s);
            }
            ++lineIdx;
        }
    }

    // Scan directory for any new files not in the list
    {
        QSet<QString> listed;
        for (const RomSection &s : romMap) listed.insert(s.fileName);

        QDirIterator it(inDir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            QString rel = QDir(inDir).relativeFilePath(it.filePath()).replace('\\', '/').toLower();
            if (rel == kHeaderFile || rel == "repack-filelist.txt") continue;
            if (!listed.contains(rel)) {
                RomSection s;
                s.fileName    = rel;
                s.pathHash    = computeFileHash(rel);
                s.dataOffset  = kFileAtEnd;
                s.origDataOffset = kFileAtEnd;
                s.isNew       = true;
                romMap.append(s);
            }
        }
    }

    bool compress            = (platform == BecPlatform::Xbox || platform == BecPlatform::PS2);
    bool includeUncompressed = (platform == BecPlatform::PS2);

    // Phase 1: sequential load + compress (I/O-bound; async caused QByteArray COW heap corruption)
    for (RomSection &s : romMap) {
        QString path = inDir + "/" + s.fileName;
        path.replace('\\', '/');
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) continue;
        QByteArray data = f.readAll();
        s.dataSize = static_cast<quint32>(data.size());

        if (compress) {
            QByteArray comp = zlibCompress(data);
            double ratio = data.isEmpty() ? 1.0
                         : double(comp.size()) / double(data.size());
            if (ratio < 0.9) {
                s.fileData       = comp;
                s.compressedSize = static_cast<quint32>(comp.size());
            } else {
                s.fileData       = data;
                s.compressedSize = 0;
            }
        } else {
            s.fileData       = data;
            s.compressedSize = 0;
        }
    }
    out(QStringLiteral("BEC pack: loaded %1 files").arg(romMap.size()));

    // Sort by (DataOffset ASC, non-duplicate first)
    std::stable_sort(romMap.begin(), romMap.end(),
        [](const RomSection &a, const RomSection &b) {
            if (a.dataOffset != b.dataOffset) return a.dataOffset < b.dataOffset;
            return (!a.isDuplicate) && b.isDuplicate;
        });

    // Phase 2: sequential offset assignment
    quint32 headerSize = 0x10 + static_cast<quint32>(romMap.size()) * 0x10;
    quint32 baseAddr   = (headerSize + fileAlignment - 1) & ~(fileAlignment - 1);
    quint32 currentAddr = baseAddr;

    RomSection *lastItem = nullptr;
    for (RomSection &s : romMap) {
        bool duplicate = lastItem && !s.isNew
                       && lastItem->origDataOffset == s.origDataOffset;
        if (duplicate) {
            s.compressedSize = lastItem->compressedSize | kDuplicateFlag;
            s.dataOffset     = lastItem->dataOffset;
        } else {
            s.dataOffset = currentAddr;
            currentAddr += s.storedSize();
            currentAddr += (fileAlignment - 1);
            currentAddr += kChecksumSize;
            currentAddr &= ~(fileAlignment - 1);

            if (includeUncompressed && s.unpackedCompSize() > 0) {
                currentAddr += s.dataSize;
                currentAddr += (fileAlignment - 1);
                currentAddr &= ~(fileAlignment - 1);
            }
        }
        lastItem = &s;
    }

    // Open output file
    QDir().mkpath(QFileInfo(outFile).absolutePath());
    QFile out_f(outFile);
    if (!out_f.open(QIODevice::WriteOnly)) {
        err(QStringLiteral("Cannot write BEC: ") + outFile);
        return false;
    }

    quint32 nrOfFiles = static_cast<quint32>(romMap.size());

    // Write BEC header: " ceb" + version(2) + alignment(2) + count(4) + magic(4)
    out_f.write(kMagic, 4);
    quint8 ver[2] = { 3, 0 }; out_f.write(reinterpret_cast<const char *>(ver), 2);
    quint8 algn[2] = { quint8(fileAlignment), quint8(fileAlignment >> 8) };
    out_f.write(reinterpret_cast<const char *>(algn), 2);
    writeU32LE(out_f, nrOfFiles);
    writeU32LE(out_f, headerMagic);

    // Sort indices by PathHash for the entry table (avoids copying all fileData QByteArrays)
    QVector<int> hashOrder(romMap.size());
    std::iota(hashOrder.begin(), hashOrder.end(), 0);
    std::sort(hashOrder.begin(), hashOrder.end(), [&romMap](int a, int b) {
        return romMap[a].pathHash < romMap[b].pathHash;
    });

    // Write entry table
    for (int idx : hashOrder) {
        const RomSection &s = romMap[idx];
        writeU32LE(out_f, s.pathHash);
        writeU32LE(out_f, s.dataOffset);
        writeU32LE(out_f, s.compressedSize);
        writeU32LE(out_f, s.dataSize);
    }

    // Align to fileAlignment after header
    alignFileWithZeros(out_f, fileAlignment);

    // Write file data sorted by DataOffset
    std::sort(romMap.begin(), romMap.end(), [](const RomSection &a, const RomSection &b) {
        return a.dataOffset < b.dataOffset;
    });

    int written = 0;
    for (const RomSection &s : romMap) {
        // Skip duplicates (same dataOffset as a previous item)
        if (static_cast<quint32>(out_f.pos()) > s.dataOffset) continue;

        out_f.write(s.fileData);

        // 8-byte checksum placeholder
        if (s.storedSize() > 0)
            out_f.write(QByteArray(kChecksumSize, '\0'));
        else
            out_f.write(QByteArray(static_cast<int>(fileAlignment), '\0'));

        alignFileWithZeros(out_f, fileAlignment);

        // PS2: also write uncompressed copy
        if (includeUncompressed && s.unpackedCompSize() > 0) {
            QByteArray uncompressed = zlibDecompress(s.fileData);
            out_f.write(uncompressed);
            alignFileWithZeros(out_f, fileAlignment);
        }

        ++written;
        if (written % 2500 == 0)
            out(QStringLiteral("BEC pack write: %1/%2").arg(written).arg(romMap.size()));
    }

    out(QStringLiteral("BEC pack complete: %1 files → %2").arg(nrOfFiles).arg(QFileInfo(outFile).fileName()));
    return true;
}

} // namespace Tools
