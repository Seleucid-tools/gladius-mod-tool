#include "UpdateStringsBin.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QStringConverter>
#include <QFileInfo>

namespace Tools {

bool updateStringsBin(const QString &configDir, const LogCb &out, const LogCb &err)
{
    const QString txtPath = configDir + "lookuptext_eng.txt";
    const QString binPath = configDir + "lookuptext_eng.bin";

    QFile fin(txtPath);
    if (!fin.open(QIODevice::ReadOnly)) {
        err(QStringLiteral("Cannot open: ") + txtPath);
        return false;
    }

    // Read file as Windows-1252 (CP1252)
    QStringDecoder decoder("Windows-1252");
    QString content = decoder(fin.readAll());
    fin.close();

    QStringList lines = content.split('\n');
    // Remove trailing empty line from split if present
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();

    if (lines.size() < 2) {
        err(QStringLiteral("lookuptext_eng.txt has too few lines"));
        return false;
    }

    // Get highest ID from last line  (format: "ID^string")
    QRegularExpression idRe(QStringLiteral("^(\\d+)\\^"));
    QRegularExpression strRe(QStringLiteral("(?<=\\^)(?!.*\\^)(.*)$"));

    auto lastMatch = idRe.match(lines.last());
    if (!lastMatch.hasMatch()) {
        err(QStringLiteral("Cannot parse ID from last line of lookuptext_eng.txt"));
        return false;
    }
    int highestNumber = lastMatch.captured(1).toInt();

    // Initial file size: 13 + highestNumber*4
    // = 2(count) + 2(zeros) + 3(fileSize) + 1(zero) + 3(addrEnd) + 1(zero)
    //   + highestNumber*4 (address table) + 1(final zero in table)
    int fileSize = 13 + highestNumber * 4;
    int addressesEndOffset = fileSize - 1;

    // Parse lines into a map of ID → string content
    // Lines 0 and 1 are headers; entries start at line 2
    QVector<QString> linesFinal(highestNumber);
    int j = 2; // index into lines array (skip 2 header lines)
    for (int i = 0; i < highestNumber; ++i) {
        int lineId = -1;
        if (j < lines.size()) {
            auto m = idRe.match(lines[j]);
            if (m.hasMatch())
                lineId = m.captured(1).toInt();
        }

        QString text;
        if (lineId == i + 1) {
            auto m = strRe.match(lines[j]);
            text = m.hasMatch() ? m.captured(1) : QString();
            text.replace(QStringLiteral("\\r\\n"), QStringLiteral("\n"));
            ++j;
        }
        linesFinal[i] = text;
        fileSize += text.length() + 1; // +1 for null terminator
    }

    QFile fout(binPath);
    if (!fout.open(QIODevice::WriteOnly)) {
        err(QStringLiteral("Cannot write: ") + binPath);
        return false;
    }

    QStringEncoder encoder("Windows-1252");

    // 2-byte count LE
    quint16 cnt = static_cast<quint16>(highestNumber + 1);
    fout.write(reinterpret_cast<const char *>(&cnt), 2);
    // 2 zero bytes
    fout.write("\x00\x00", 2);
    // 3-byte fileSize LE + 1 zero
    fout.write(reinterpret_cast<const char *>(&fileSize), 3);
    fout.write("\x00", 1);
    // 3-byte addressesEndOffset LE + 1 zero
    fout.write(reinterpret_cast<const char *>(&addressesEndOffset), 3);
    fout.write("\x00", 1);

    // Address table: one entry per string, 3-byte offset LE + 1 zero
    int offset = addressesEndOffset + 1;
    for (int i = 0; i < highestNumber; ++i) {
        fout.write(reinterpret_cast<const char *>(&offset), 3);
        fout.write("\x00", 1);
        offset += linesFinal[i].length() + 1;
    }
    // Final zero byte closing the table
    fout.write("\x00", 1);

    // String data: each string encoded as CP1252 + null terminator
    for (int i = 0; i < highestNumber; ++i) {
        QByteArray encoded = encoder(linesFinal[i]);
        fout.write(encoded);
        fout.write("\x00", 1);
    }

    fout.close();
    out(QStringLiteral("Written lookuptext_eng.bin (%1 strings)").arg(highestNumber));
    return true;
}

} // namespace Tools
