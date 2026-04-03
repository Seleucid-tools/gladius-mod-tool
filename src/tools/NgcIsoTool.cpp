#include "NgcIsoTool.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QFileInfo>
#include <cstring>

namespace Tools {

// ── Helpers ───────────────────────────────────────────────────────────────────

static quint32 readU32BE(QFile &f, qint64 offset)
{
    f.seek(offset);
    quint8 buf[4]; f.read(reinterpret_cast<char *>(buf), 4);
    return (quint32(buf[0]) << 24) | (quint32(buf[1]) << 16)
         | (quint32(buf[2]) << 8)  | quint32(buf[3]);
}

static quint8 readU8(QFile &f, qint64 offset)
{
    f.seek(offset);
    quint8 b; f.read(reinterpret_cast<char *>(&b), 1);
    return b;
}

static QString readString(QFile &f, qint64 offset, int maxLen)
{
    f.seek(offset);
    QByteArray buf = f.read(maxLen);
    QString s;
    for (char c : buf) {
        if (c == '\0') break;
        s += QChar(static_cast<quint8>(c));
    }
    return s;
}

static QString readString0(QFile &f, qint64 offset, int maxLen)
{
    return readString(f, offset, maxLen);
}

static quint32 alignAdr(quint32 adr, quint32 alignVal)
{
    if (adr & (alignVal - 1)) {
        adr &= ~(alignVal - 1);
        adr += alignVal;
    }
    return adr;
}

static bool writeSection(QFile &src, const QString &dir, const QString &filename,
                         qint64 addr, qint64 size)
{
    if (size < 0) return true;
    src.seek(addr);
    QByteArray data = src.read(size);

    QString fullPath = dir + filename;
    QDir().mkpath(QFileInfo(fullPath).absolutePath());
    QFile out(fullPath);
    if (!out.open(QIODevice::WriteOnly)) return false;
    out.write(data);
    return true;
}

static void alignWithZeros(QFile &f, quint32 alignment)
{
    qint64 pos = f.pos();
    quint32 target = alignAdr(static_cast<quint32>(pos), alignment);
    qint64 pad = target - pos;
    if (pad > 0)
        f.write(QByteArray(static_cast<int>(pad), '\0'));
}

// ── IsoSection ────────────────────────────────────────────────────────────────

struct IsoSection {
    QString name;
    quint32 address;
    int     fileID;
    quint32 size;
};

// ── DirName ───────────────────────────────────────────────────────────────────

struct DirName {
    QString name;
    quint32 startID;
    quint32 endID;
};

// ── fstDir (FST tree node) ────────────────────────────────────────────────────

struct fstDir {
    QString name;
    int     flag;    // 1=directory, 0=file
    int     fileID;
    quint32 fileSize;
    quint32 fileOffset;
    QVector<fstDir *> subDirs;

    fstDir(const QString &n, int fl, int fid = 0, quint32 fsz = 0)
        : name(n), flag(fl), fileID(fid), fileSize(fsz), fileOffset(0) {}

    ~fstDir() { qDeleteAll(subDirs); }

    fstDir *getSubDir(const QString &n) const {
        for (fstDir *d : subDirs)
            if (d->name == n) return d;
        return nullptr;
    }

    fstDir *addSubDir(const QString &n) {
        fstDir *existing = getSubDir(n);
        if (!existing) {
            subDirs.append(new fstDir(n, 1));
            std::sort(subDirs.begin(), subDirs.end(), [](const fstDir *a, const fstDir *b) {
                return a->name.toLower().replace('_', '}') < b->name.toLower().replace('_', '}');
            });
        }
        return getSubDir(n);
    }

    fstDir *addFile(const QString &n, int fid, quint32 fsz) {
        fstDir *existing = getSubDir(n);
        if (!existing) {
            subDirs.append(new fstDir(n, 0, fid, fsz));
            std::sort(subDirs.begin(), subDirs.end(), [](const fstDir *a, const fstDir *b) {
                return a->name.toLower().replace('_', '}') < b->name.toLower().replace('_', '}');
            });
        }
        return getSubDir(n);
    }

    int getNrOfEntries() const {
        int n = 0;
        for (fstDir *d : subDirs) {
            if (d->fileID != -1) ++n;
            n += d->getNrOfEntries();
        }
        return n;
    }

    int getLengthOfStrings() const {
        int l = name.length() + 1;
        for (fstDir *d : subDirs) {
            if (d->fileID != -1) l += d->getLengthOfStrings();
        }
        return l;
    }

    quint32 setOffset(quint32 off) {
        fileOffset = off;
        return off + fileSize;
    }

    void setFileSize(quint32 fsz) { fileSize = fsz; }

    // Returns (fstWords, stringData)
    QPair<QVector<quint32>, QByteArray> createFST(int strTableLen, int parentID, int ownID) const
    {
        // Skip system files and gap-fillers (fileID==-1, non-empty name).
        // Root also has fileID==-1 but has an empty name and must be serialised.
        if (fileID == -1 && !name.isEmpty()) return {};

        quint32 word2 = 0, word3 = 0;
        QByteArray string;
        if (!name.isEmpty()) {
            string = name.toUtf8() + '\0';
            if (flag == 0) {
                word3 = fileSize;
                word2 = fileOffset;
            } else {
                word3 = static_cast<quint32>(ownID + getNrOfEntries() + 1);
                word2 = static_cast<quint32>(parentID);
            }
        } else {
            // root
            word3 = static_cast<quint32>(getNrOfEntries() + 1);
        }

        QVector<quint32> fst;
        fst.append(static_cast<quint32>(flag << 24) | static_cast<quint32>(strTableLen));
        fst.append(word2);
        fst.append(word3);

        int n = 0;
        for (fstDir *d : subDirs) {
            auto [subFst, subStr] = d->createFST(strTableLen + string.size(), ownID, ownID + n + 1);
            fst += subFst;
            string += subStr;
            if (!subStr.isEmpty())
                n += 1 + d->getNrOfEntries();
        }
        return {fst, string};
    }
};

// ── UNPACK ────────────────────────────────────────────────────────────────────

static void parseDOL(QFile &f, quint32 base, const QString &filedir,
                     QVector<IsoSection> &romMap, const LogCb &out)
{
    quint32 totSize = 0x100;
    QVector<quint32> textPos(6), textMem(6), textSize(6);
    QVector<quint32> dataPos(10), dataMem(10), dataSize(10);

    for (int i = 0; i < 6; ++i) {
        textPos[i]  = readU32BE(f, base + 0x00 + 4 * i);
        textMem[i]  = readU32BE(f, base + 0x48 + 4 * i);
        textSize[i] = readU32BE(f, base + 0x90 + 4 * i);
        totSize += textSize[i];
    }
    for (int i = 0; i < 10; ++i) {
        dataPos[i]  = readU32BE(f, base + 0x1c + 4 * i);
        dataMem[i]  = readU32BE(f, base + 0x64 + 4 * i);
        dataSize[i] = readU32BE(f, base + 0xac + 4 * i);
        totSize += dataSize[i];
    }
    for (int i = 0; i < 6; ++i)
        writeSection(f, filedir + "code/", "Text_0x" + QString::number(textMem[i], 16) + ".bin",
                     base + textPos[i], textSize[i]);
    for (int i = 0; i < 10; ++i)
        writeSection(f, filedir + "code/", "Data_0x" + QString::number(dataMem[i], 16) + ".bin",
                     base + dataPos[i], dataSize[i]);

    totSize = alignAdr(totSize, 0x100);
    writeSection(f, filedir, "bootfile.dol", base, totSize);
    romMap.append({QStringLiteral("/bootfile.dol"), base, -1, totSize});
}

static void parseFST(QFile &f, quint32 base, const QString &filedir,
                     QVector<IsoSection> &romMap, QVector<DirName> &dirNames,
                     const LogCb &out)
{
    quint32 numEntries = readU32BE(f, base + 0x8);
    quint32 stringTable = numEntries * 0xc;

    auto getDirPath = [&](int fileID) {
        QString path;
        for (const DirName &d : dirNames) {
            if (fileID >= (int)d.startID && fileID < (int)d.endID)
                path += d.name + '/';
        }
        return path;
    };

    for (quint32 i = 0; i < numEntries; ++i) {
        quint32 off = 0xc * i;
        quint8 flag = readU8(f, base + off);
        quint32 strOff = (readU32BE(f, base + off) & 0xffffff) + stringTable;
        quint32 word2  = readU32BE(f, base + off + 4);
        quint32 word3  = readU32BE(f, base + off + 8);

        if (flag == 1) {
            // Root entry (i==0) must have an empty name: its str_offset=0 points to the
            // first real filename in the string table (e.g. "audio.bec"), not the root's
            // name. Python explicitly forces string="" when offset==0. If we read it here
            // we prepend that filename to every extracted path and block all other writes.
            QString name = (i == 0) ? QString() : readString0(f, base + strOff, 32);
            dirNames.append({name, static_cast<quint32>(off / 0xc), word3});
        } else if (flag == 0) {
            QString name = readString0(f, base + strOff, 32);
            if (name.isEmpty())
                name = "Unknown_" + QString::number(word2, 16) + ".bin";
            QString path = getDirPath(static_cast<int>(i));
            romMap.append({path + name, word2, static_cast<int>(i), word3});
            writeSection(f, (filedir + path).replace("//", "/"),
                         name.replace("//", "/"), word2, word3);
        }
    }
}

bool ngcIsoUnpack(const QString &isoPath, const QString &outDir,
                  const QString &outFileList,
                  const LogCb &out, const LogCb &err)
{
    QFile f(isoPath);
    if (!f.open(QIODevice::ReadOnly)) {
        err(QStringLiteral("Cannot open ISO: ") + isoPath);
        return false;
    }

    QDir().mkpath(outDir);

    QString header;
    header += isoPath + '\n';
    header += "FileSize: " + QString::number(f.size(), 16) + '\n';
    header += "\nGame Code:   " + readString(f, 0x0,  4);
    header += "\nMaker Code:  " + readString(f, 0x4,  2);
    header += "\nGame Name:   " + readString(f, 0x20, 0x3e0);

    QVector<IsoSection> romMap;
    QVector<DirName>    dirNames;

    writeSection(f, outDir, "boot.bin", 0x0,   0x440);  romMap.append({"/boot.bin", 0x0, -1, 0x440});
    writeSection(f, outDir, "bi2.bin",  0x440, 0x2000); romMap.append({"/bi2.bin", 0x440, -1, 0x2000});

    quint32 appsize = readU32BE(f, 0x2440 + 0x14) + readU32BE(f, 0x2440 + 0x18);
    writeSection(f, outDir, "appldr.bin", 0x2440, appsize);
    romMap.append({"/appldr.bin", 0x2440, -1, alignAdr(0x2440 + appsize, 0x100) - 0x2440});

    quint32 fstOffset = readU32BE(f, 0x424);
    quint32 fstSize   = readU32BE(f, 0x428);
    writeSection(f, outDir, "fst.bin", fstOffset, fstSize);
    romMap.append({"/fst.bin", fstOffset, -1, fstSize});

    parseDOL(f, readU32BE(f, 0x420), outDir, romMap, out);
    parseFST(f, fstOffset, outDir, romMap, dirNames, out);

    // Build file list
    std::sort(romMap.begin(), romMap.end(), [](const IsoSection &a, const IsoSection &b) {
        return a.address < b.address;
    });
    QString fileList;
    quint32 prevEnd = 0;
    for (const IsoSection &s : romMap) {
        if (s.address > prevEnd) {
            fileList += QString::number(prevEnd, 16) + " /Unknown_"
                      + QString::number(prevEnd, 16) + ".bin -1 "
                      + QString::number(s.address - prevEnd, 16) + '\n';
        }
        fileList += QString::number(s.address, 16) + ' ' + s.name
                  + ' ' + QString::number(s.fileID, 16)
                  + ' ' + QString::number(s.size, 16) + '\n';
        prevEnd = alignAdr(s.address + s.size, 4);
    }

    // Write header
    {
        QFile hf(outDir + "_Header.txt");
        hf.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream ts(&hf); ts.setEncoding(QStringConverter::Utf8);
        ts << header << "\n\n" << fileList;
    }

    // Write file list
    if (!outFileList.isEmpty()) {
        QFile lf(outDir + outFileList);
        lf.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream ts(&lf); ts.setEncoding(QStringConverter::Utf8);
        ts << fileList;
    }

    out(QStringLiteral("NGC ISO unpacked: %1 sections").arg(romMap.size()));
    return true;
}

// ── PACK ──────────────────────────────────────────────────────────────────────

static quint32 calcFSTSize(const fstDir &root)
{
    return static_cast<quint32>(root.getNrOfEntries() * 0xc + 0xc
                                + root.getLengthOfStrings() - 1);
}

static void addFileToFST(fstDir &root, const QString &path, int fileID, quint32 fileSize)
{
    fstDir *cur = &root;
    QString remaining = path;
    if (remaining.startsWith('/')) remaining = remaining.mid(1);
    while (!remaining.isEmpty()) {
        int pos = remaining.indexOf('/');
        if (pos != -1) {
            cur = cur->addSubDir(remaining.left(pos));
            remaining = remaining.mid(pos + 1);
        } else {
            cur->addFile(remaining, fileID, fileSize);
            break;
        }
    }
}

static fstDir *findNode(fstDir &root, const QString &path)
{
    fstDir *cur = &root;
    QString remaining = path;
    if (remaining.startsWith('/')) remaining = remaining.mid(1);
    while (!remaining.isEmpty()) {
        int pos = remaining.indexOf('/');
        QString part = (pos != -1) ? remaining.left(pos) : remaining;
        cur = cur->getSubDir(part);
        if (!cur) return nullptr;
        remaining = (pos != -1) ? remaining.mid(pos + 1) : QString();
    }
    return cur;
}

bool ngcIsoPack(const QString &inDir, const QString &fstFile,
                const QString &fstMap, const QString &outIso,
                const LogCb &out, const LogCb &err)
{
    // Read file list
    QFile mapFile(fstMap);
    if (!mapFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err(QStringLiteral("Cannot open fst map: ") + fstMap);
        return false;
    }

    struct MapEntry { quint32 address; QString path; int fileID; quint32 size; };
    QVector<MapEntry> entries;
    {
        QTextStream in(&mapFile);
        while (!in.atEnd()) {
            QStringList words = in.readLine().split(' ', Qt::SkipEmptyParts);
            if (words.size() == 4) {
                quint32 addr = words[0].toUInt(nullptr, 16);
                QString path = words[1];
                int fid      = words[2].toInt(nullptr, 16);
                quint32 sz   = words[3].toUInt(nullptr, 16);
                entries.append({addr, path, fid, sz});
            }
        }
    }

    fstDir root("", 1, -1, 0);
    for (auto &e : entries) {
        // Use on-disk size if the file exists; fall back to the size from the filelist
        // (Unknown_*.bin gap-filler entries are never extracted to disk).
        QFileInfo fi(inDir + e.path);
        if (fi.exists()) e.size = static_cast<quint32>(fi.size());
        addFileToFST(root, e.path, e.fileID, e.size);
    }

    // Update fst.bin size
    {
        fstDir *node = findNode(root, "/fst.bin");
        if (node) node->setFileSize(alignAdr(calcFSTSize(root), 4));
    }

    // Set file offsets (sequential pass)
    quint32 offset = 0;
    for (auto &e : entries) {
        fstDir *node = findNode(root, e.path);
        if (node) offset = node->setOffset(offset);
        if (e.path == "/appldr.bin" || e.path == "/bootfile.dol")
            offset = alignAdr(offset, 0x100);
        offset = alignAdr(offset, 4);
    }

    // Build FST binary
    auto [fstWords, fstStrings] = root.createFST(0, 0, 0);
    {
        QFile ff(fstFile);
        QDir().mkpath(QFileInfo(fstFile).absolutePath());
        if (!ff.open(QIODevice::WriteOnly)) { err(QStringLiteral("Cannot write fst: ") + fstFile); return false; }
        for (quint32 w : fstWords) {
            quint8 buf[4] = { quint8(w >> 24), quint8(w >> 16), quint8(w >> 8), quint8(w) };
            ff.write(reinterpret_cast<const char *>(buf), 4);
        }
        ff.write(fstStrings);
    }

    // Write ISO
    QDir().mkpath(QFileInfo(outIso).absolutePath());
    QFile iso(outIso);
    if (!iso.open(QIODevice::WriteOnly)) { err(QStringLiteral("Cannot write ISO: ") + outIso); return false; }

    qint64 bootfileOffset = 0, fstOffset = 0, fstSize = 0;
    for (const auto &e : entries) {
        QString src = inDir + e.path;
        QFile sf(src);

        if (e.path == "/bootfile.dol") bootfileOffset = iso.pos();
        if (e.path == "/fst.bin")      fstOffset      = iso.pos();

        if (!sf.open(QIODevice::ReadOnly)) {
            // Gap-filler entries (Unknown_*.bin) were never extracted; write zeros.
            iso.write(QByteArray(static_cast<int>(e.size), '\0'));
        } else {
            QByteArray data = sf.readAll();
            if (e.path == "/fst.bin") fstSize = data.size();
            iso.write(data);
        }

        if (e.path == "/appldr.bin" || e.path == "/bootfile.dol")
            alignWithZeros(iso, 0x100);
        else
            alignWithZeros(iso, 0x4);

        out(QStringLiteral("wrote ") + e.path);
    }

    // Patch DOL offset, FST offset, FST size, and max FST size into boot.bin header.
    // GC header layout: 0x420=DOL offset, 0x424=FST offset, 0x428=FST size, 0x42C=max FST size.
    iso.seek(0x420);
    quint8 buf[16] = {
        quint8(bootfileOffset >> 24), quint8(bootfileOffset >> 16),
        quint8(bootfileOffset >> 8),  quint8(bootfileOffset),
        quint8(fstOffset >> 24),      quint8(fstOffset >> 16),
        quint8(fstOffset >> 8),       quint8(fstOffset),
        quint8(fstSize >> 24),        quint8(fstSize >> 16),
        quint8(fstSize >> 8),         quint8(fstSize),
        quint8(fstSize >> 24),        quint8(fstSize >> 16),
        quint8(fstSize >> 8),         quint8(fstSize)
    };
    iso.write(reinterpret_cast<const char *>(buf), 16);

    out(QStringLiteral("NGC ISO packed: ") + QFileInfo(outIso).fileName());
    return true;
}

} // namespace Tools
