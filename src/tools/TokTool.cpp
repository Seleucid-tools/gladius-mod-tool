#include "TokTool.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QVector>
#include <QHash>
#include <algorithm>

namespace Tools {

// ── Helpers ───────────────────────────────────────────────────────────────────

static quint32 read24LE(const QByteArray &data, int pos)
{
    quint8 b0 = static_cast<quint8>(data[pos]);
    quint8 b1 = static_cast<quint8>(data[pos + 1]);
    quint8 b2 = static_cast<quint8>(data[pos + 2]);
    return b0 | (quint32(b1) << 8) | (quint32(b2) << 16);
}

static void write16LE(QByteArray &out, quint16 v)
{
    out.append(static_cast<char>(v & 0xff));
    out.append(static_cast<char>((v >> 8) & 0xff));
}

static void write24LE(QByteArray &out, quint32 v)
{
    out.append(static_cast<char>(v & 0xff));
    out.append(static_cast<char>((v >> 8) & 0xff));
    out.append(static_cast<char>((v >> 16) & 0xff));
}

static QString readTerminatedString(const QByteArray &data, int offset)
{
    QString result;
    while (offset < data.size()) {
        quint8 c = static_cast<quint8>(data[offset++]);
        if (c >= 128) {
            result += QChar(c - 128);
            break;
        }
        result += QChar(c);
    }
    return result;
}

// ── Decompress ────────────────────────────────────────────────────────────────

bool tokDecompress(const QString &inStrings, const QString &inLines,
                   const QString &inMain,   const QString &outTok,
                   const LogCb &out, const LogCb &err)
{
    QFile sf(inStrings), lf(inLines), mf(inMain);
    if (!sf.open(QIODevice::ReadOnly) || !lf.open(QIODevice::ReadOnly) || !mf.open(QIODevice::ReadOnly)) {
        err(QStringLiteral("tok-tool: cannot open input files"));
        return false;
    }
    QByteArray strData = sf.readAll();
    QByteArray lineData = lf.readAll();
    QByteArray mainData = mf.readAll();

    // Read string offset table (3 bytes each)
    int nrOfStrings = (strData.size() >= 3) ? static_cast<int>(read24LE(strData, 0) / 3) : 0;
    QVector<quint32> stringOffsets(nrOfStrings);
    for (int i = 0; i < nrOfStrings; ++i)
        stringOffsets[i] = read24LE(strData, i * 3);

    // Decode strings
    QVector<QString> strings(nrOfStrings);
    for (int i = 0; i < nrOfStrings; ++i)
        strings[i] = readTerminatedString(strData, static_cast<int>(stringOffsets[i]));

    // Read line offset table (4 bytes each: 3-byte offset + 1 byte word count)
    int nrOfLines = (lineData.size() >= 3) ? static_cast<int>(read24LE(lineData, 0) / 4) : 0;
    QVector<quint32> lineOffsets(nrOfLines);
    QVector<int> lineWordCounts(nrOfLines);
    for (int i = 0; i < nrOfLines; ++i) {
        lineOffsets[i] = read24LE(lineData, i * 4);
        lineWordCounts[i] = static_cast<quint8>(lineData[i * 4 + 3]);
    }

    // Decode lines
    QVector<QString> lines(nrOfLines);
    for (int i = 0; i < nrOfLines; ++i) {
        QString sentence;
        int pos = static_cast<int>(lineOffsets[i]);
        for (int j = 0; j < lineWordCounts[i]; ++j) {
            if (pos >= lineData.size()) break;
            quint8 byte1 = static_cast<quint8>(lineData[pos++]);
            int idx = byte1;
            if (byte1 >= 128) {
                if (pos >= lineData.size()) break;
                quint8 byte2 = static_cast<quint8>(lineData[pos++]);
                idx = (byte1 - 128) + ((byte2 - 1) << 7);
            }
            if (idx < strings.size())
                sentence += strings[idx] + ' ';
        }
        lines[i] = sentence;
    }

    // Decode main file and write output
    QFile of(outTok);
    if (!of.open(QIODevice::WriteOnly)) {
        err(QStringLiteral("tok-tool: cannot write ") + outTok);
        return false;
    }
    QTextStream os(&of);
    os.setEncoding(QStringConverter::Utf8);

    int pos = 0;
    while (pos < mainData.size()) {
        quint8 byte1 = static_cast<quint8>(mainData[pos++]);
        int idx = byte1;
        if (byte1 >= 128) {
            if (pos >= mainData.size()) break;
            quint8 byte2 = static_cast<quint8>(mainData[pos++]);
            idx = (byte1 - 128) + ((byte2 - 1) << 7);
        }
        if (idx < lines.size())
            os << lines[idx].trimmed() << '\n';
    }

    out(QStringLiteral("tok-tool: decompressed → ") + QFileInfo(outTok).fileName());
    return true;
}

// ── Compress ──────────────────────────────────────────────────────────────────

bool tokCompress(const QString &inputTok,
                 const QString &outStrings, const QString &outLines, const QString &outMain,
                 const LogCb &out, const LogCb &err)
{
    QFile fin(inputTok);
    if (!fin.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err(QStringLiteral("tok-tool: cannot open ") + inputTok);
        return false;
    }
    QTextStream in(&fin);
    in.setEncoding(QStringConverter::Utf8);

    // Build word and line frequency maps; also keep ordered file lines
    QHash<QString, int> wordFreq;
    QHash<QString, int> lineFreq;
    QVector<QString> fileLines;

    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.trimmed().isEmpty() || line.startsWith(QStringLiteral("//")))
            continue;

        QString normalized;
        const QStringList words = line.split(' ', Qt::SkipEmptyParts);
        for (QString word : words) {
            word = word.trimmed().remove(QChar(','));
            if (word == QStringLiteral("//"))
                break;
            wordFreq[word]++;
            normalized += word + ' ';
        }
        lineFreq[normalized]++;
        fileLines.append(normalized);
    }

    // Sort words by (frequency DESC, word DESC) — mirrors Python's
    // sorted(dic.items(), key=itemgetter(1,0), reverse=True)
    QVector<QPair<QString, int>> sortedWords;
    sortedWords.reserve(wordFreq.size());
    for (auto it = wordFreq.begin(); it != wordFreq.end(); ++it)
        sortedWords.append({it.key(), it.value()});
    std::stable_sort(sortedWords.begin(), sortedWords.end(),
        [](const QPair<QString,int> &a, const QPair<QString,int> &b) {
            if (a.second != b.second) return a.second > b.second;
            return a.first > b.first;
        });

    // Assign word indices
    QHash<QString, int> wordIndex;
    wordIndex.reserve(sortedWords.size());
    for (int i = 0; i < sortedWords.size(); ++i)
        wordIndex[sortedWords[i].first] = i;

    int nrOfStrings = sortedWords.size();

    // Encode each line as byte sequence of word indices
    QVector<QPair<QString, int>> sortedLines;
    sortedLines.reserve(lineFreq.size());
    for (auto it = lineFreq.begin(); it != lineFreq.end(); ++it)
        sortedLines.append({it.key(), it.value()});
    std::stable_sort(sortedLines.begin(), sortedLines.end(),
        [](const QPair<QString,int> &a, const QPair<QString,int> &b) {
            if (a.second != b.second) return a.second > b.second;
            return a.first > b.first;
        });

    // Pre-compute byte sequences for each line
    QVector<QByteArray> lineBytes(sortedLines.size());
    for (int i = 0; i < sortedLines.size(); ++i) {
        QByteArray bytes;
        const QStringList ws = sortedLines[i].first.split(' ', Qt::SkipEmptyParts);
        for (const QString &w : ws) {
            int idx = wordIndex.value(w, 0);
            if (idx >= 128) {
                bytes.append(static_cast<char>((idx % 128) | 0x80));
                bytes.append(static_cast<char>(idx >> 7));
            } else {
                bytes.append(static_cast<char>(idx));
            }
        }
        lineBytes[i] = bytes;
    }

    // Assign line indices
    QHash<QString, int> lineIndex;
    lineIndex.reserve(sortedLines.size());
    for (int i = 0; i < sortedLines.size(); ++i)
        lineIndex[sortedLines[i].first] = i;

    // ── Write strings file ─────────────────────────────────────────────────────
    QByteArray strFile;
    // Offset table: nrOfStrings × 3 bytes, starting offset = nrOfStrings*3
    quint32 strOffset = static_cast<quint32>(nrOfStrings * 3);
    for (int i = 0; i < nrOfStrings; ++i) {
        write16LE(strFile, static_cast<quint16>(strOffset & 0xffff));
        strFile.append(static_cast<char>((strOffset >> 16) & 0xff));
        strOffset += static_cast<quint32>(sortedWords[i].first.length());
    }
    // String data: all chars except last written as-is; last char | 0x80
    for (int i = 0; i < nrOfStrings; ++i) {
        const QString &word = sortedWords[i].first;
        QByteArray encoded = word.toUtf8();
        if (!encoded.isEmpty()) {
            for (int j = 0; j < encoded.size() - 1; ++j)
                strFile.append(encoded[j]);
            strFile.append(static_cast<char>(static_cast<quint8>(encoded.back()) | 0x80));
        }
    }

    // ── Write lines file ───────────────────────────────────────────────────────
    QByteArray lineFile;
    int nrOfLines = sortedLines.size();
    // Offset table: nrOfLines × 4 bytes, starting offset = nrOfLines*4
    quint32 lineOff = static_cast<quint32>(nrOfLines * 4);
    for (int i = 0; i < nrOfLines; ++i) {
        write16LE(lineFile, static_cast<quint16>(lineOff & 0xffff));
        lineFile.append(static_cast<char>((lineOff >> 16) & 0xff));
        const QStringList ws = sortedLines[i].first.split(' ', Qt::SkipEmptyParts);
        lineFile.append(static_cast<char>(ws.size() & 0xff));
        lineOff += static_cast<quint32>(lineBytes[i].size());
    }
    // Byte sequences
    for (const QByteArray &lb : lineBytes)
        lineFile.append(lb);

    // ── Write main file ────────────────────────────────────────────────────────
    QByteArray mainFile;
    for (const QString &fl : fileLines) {
        int idx = lineIndex.value(fl, 0);
        if (idx >= 128) {
            mainFile.append(static_cast<char>((idx % 128) | 0x80));
            mainFile.append(static_cast<char>(idx >> 7));
        } else {
            mainFile.append(static_cast<char>(idx));
        }
    }

    // ── Write output files ─────────────────────────────────────────────────────
    auto writeFile = [&](const QString &path, const QByteArray &data) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            err(QStringLiteral("tok-tool: cannot write ") + path);
            return false;
        }
        f.write(data);
        return true;
    };

    if (!writeFile(outStrings, strFile)) return false;
    if (!writeFile(outLines,   lineFile)) return false;
    if (!writeFile(outMain,    mainFile)) return false;

    out(QStringLiteral("tok-tool: compressed %1 lines, %2 unique words, %3 unique lines")
        .arg(fileLines.size()).arg(nrOfStrings).arg(nrOfLines));
    return true;
}

} // namespace Tools
