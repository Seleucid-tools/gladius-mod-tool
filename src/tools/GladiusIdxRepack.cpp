#include "GladiusIdxRepack.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDir>

namespace Tools {

static const QStringList kAffinity = {
    QStringLiteral("None"), QStringLiteral("Air"), QStringLiteral("Dark"),
    QStringLiteral("Earth"), QStringLiteral("Fire"),
    QStringLiteral("5 - this should not appear; if it does, something broke"),
    QStringLiteral("Water")
};

static QStringList readLines(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    QStringList result;
    while (!in.atEnd()) result.append(in.readLine());
    return result;
}

static QStringList parseTokEntries(const QString &path, const QRegularExpression &re, int group = 1)
{
    QStringList result;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) {
        auto m = re.match(in.readLine());
        if (m.hasMatch()) result.append(m.captured(group));
    }
    return result;
}

bool idxRepack(const QString &dataDir, const LogCb &out, const LogCb &err)
{
    const QString units  = dataDir + "units/";
    const QString config = dataDir + "config/";

    // ── Load text input files ──────────────────────────────────────────────────
    QStringList gladData = readLines(units + "gladiators.txt");
    QString gladRaw;
    {
        QFile f(units + "gladiators.txt");
        f.open(QIODevice::ReadOnly); gladRaw = QString::fromUtf8(f.readAll());
    }

    QStringList tintsRaw   = readLines(units + "tints.txt");
    QStringList skillsetsRaw = readLines(units + "skillsets.txt");
    QStringList statsetsRaw  = readLines(units + "statsets.txt");
    QStringList itemsetsRaw  = readLines(units + "itemsets.txt");
    QStringList schoolsRaw   = readLines(units + "schools.txt");

    // ── Parse classes/skills/items from .tok ──────────────────────────────────
    QStringList classes = parseTokEntries(config + "classdefs.tok",
        QRegularExpression(QStringLiteral("^CREATECLASS: (.*?)$")));
    QStringList skills  = parseTokEntries(config + "skills.tok",
        QRegularExpression(QStringLiteral("^SKILLCREATE: \"(.*?)\",$")));
    QStringList items   = parseTokEntries(config + "items.tok",
        QRegularExpression(QStringLiteral("^ITEMCREATE: \"(.*?)\",")));

    // ── Parse gladiators.txt ──────────────────────────────────────────────────
    QRegularExpression nameRe  (QStringLiteral("(?s)Name: (.*?)\\nClass"));
    QRegularExpression classRe (QStringLiteral("^Class: (.*?)$"));
    QRegularExpression outfitRe(QStringLiteral("^Outfit: (.*?)$"));
    QRegularExpression affinRe (QStringLiteral("^Affinity: (.*?)$"));
    QRegularExpression tintRe  (QStringLiteral("^Tint set: (.*?)$"));
    QRegularExpression skillRe (QStringLiteral("^Skill set: (.*?)$"));
    QRegularExpression statRe  (QStringLiteral("^Stat set: (.*?)$"));
    QRegularExpression itemRe  (QStringLiteral("^Item set: (.*?)$"));
    QRegularExpression schoolRe(QStringLiteral("^School: (.*?)$"));

    QStringList unitNames;
    QVector<int> nameOffsets;
    {
        auto it = nameRe.globalMatch(gladRaw);
        int off = 1;
        while (it.hasNext()) {
            auto m = it.next();
            unitNames.append(m.captured(1));
            nameOffsets.append(off);
            off += m.captured(1).length() + 1;
        }
    }
    int numEntries = unitNames.size();

    auto parseField = [&](const QRegularExpression &re) {
        QStringList result;
        for (const QString &line : gladData) {
            auto m = re.match(line);
            if (m.hasMatch()) result.append(m.captured(1));
        }
        return result;
    };

    QStringList unitClassText = parseField(classRe);
    QStringList unitOutfits   = parseField(outfitRe);
    QStringList unitAffinities = parseField(affinRe);
    QStringList unitTints     = parseField(tintRe);
    QStringList unitSkillsets = parseField(skillRe);
    QStringList unitStatsets  = parseField(statRe);
    QStringList unitItemsets  = parseField(itemRe);
    QStringList unitSchools   = parseField(schoolRe);

    // Build unitUnitsRaw entries (2+1+1+2+2+2+2 = 12 bytes, padded to 14 with school offset)
    // Actually each entry is 14 bytes: name(2) + class(1) + outfit/aff(1) + tint(2) + skill(2) + stat(2) + item(2) + school(2)
    QVector<QByteArray> unitRaw(numEntries);
    for (int i = 0; i < numEntries; ++i) {
        QByteArray entry;
        // name offset (2 bytes BE)
        quint16 nameOff = static_cast<quint16>(nameOffsets[i]);
        entry.append(static_cast<char>(nameOff >> 8));
        entry.append(static_cast<char>(nameOff & 0xff));
        unitRaw[i] = entry;
    }

    // Append class byte
    for (int i = 0; i < numEntries; ++i) {
        int ci = classes.indexOf(unitClassText[i]);
        unitRaw[i].append(static_cast<char>(ci < 0 ? 0 : ci));
    }

    // Append outfit/affinity byte
    for (int i = 0; i < numEntries; ++i) {
        int outfit = unitOutfits[i].toInt();
        int afIdx  = kAffinity.indexOf(unitAffinities[i]);
        if (afIdx < 0) { err(QStringLiteral("Invalid affinity: ") + unitAffinities[i]); afIdx = 0; }
        quint8 byte = static_cast<quint8>((outfit << 3) | (afIdx & 0x07));
        unitRaw[i].append(static_cast<char>(byte));
    }

    // ── Parse tints.txt ────────────────────────────────────────────────────────
    QStringList tintComponents;
    QRegularExpression tintCompRe(
        QStringLiteral("^(?:Skin|Hair|Cloth1|Cloth2|Armor1|Armor2): (\\d+) (\\d+) (\\d+)$"));
    QString tintsHeader;
    {
        auto m = QRegularExpression(QStringLiteral(": (.*?)$")).match(tintsRaw.isEmpty() ? QString() : tintsRaw[0]);
        tintsHeader = m.hasMatch() ? m.captured(1) : QStringLiteral("0");
    }
    for (const QString &line : tintsRaw) {
        auto m = tintCompRe.match(line);
        if (m.hasMatch()) {
            tintComponents.append(m.captured(1));
            tintComponents.append(m.captured(2));
            tintComponents.append(m.captured(3));
        }
    }

    // Append tint offset bytes
    for (int i = 0; i < numEntries; ++i) {
        int t = unitTints[i].toInt();
        quint16 toff = (t == 0) ? 0 : static_cast<quint16>((t - 1) * 18 + 1);
        unitRaw[i].append(static_cast<char>(toff >> 8));
        unitRaw[i].append(static_cast<char>(toff & 0xff));
    }

    // ── Parse skillsets.txt ────────────────────────────────────────────────────
    QVector<QByteArray> skillsets;
    QVector<int> skillsetOffsets = {1};
    {
        QByteArray cur;
        int i = 0;
        QRegularExpression setRe(QStringLiteral("^Skillset (\\d+):"));
        QRegularExpression entryRe(QStringLiteral("^(\\d+) (\\d+) (.+)$"));
        for (const QString &line : skillsetsRaw) {
            auto ms = setRe.match(line);
            if (ms.hasMatch() && ms.captured(1).toInt() == i + 1) {
                skillsets.append(cur);
                skillsetOffsets.append(2 + cur.size() + skillsetOffsets[i]);
                cur.clear(); ++i;
                continue;
            }
            auto me = entryRe.match(line);
            if (me.hasMatch()) {
                int minLv = me.captured(1).toInt();
                int maxLv = me.captured(2).toInt();
                int si    = skills.indexOf(me.captured(3).trimmed());
                cur.append(static_cast<char>(minLv));
                cur.append(static_cast<char>(maxLv));
                cur.append(static_cast<char>(si >> 8));
                cur.append(static_cast<char>(si & 0xff));
            }
        }
        skillsets.append(cur);
    }

    // ── Parse statsets.txt ─────────────────────────────────────────────────────
    QVector<QByteArray> statsets;
    QVector<int> statsetOffsets = {1};
    {
        QByteArray cur;
        int i = 0;
        QRegularExpression setRe(QStringLiteral("^Statset (\\d+):$"));
        QRegularExpression entryRe(QStringLiteral("^\\d+: (\\d+) (\\d+) (\\d+) (\\d+) (\\d+)$"));
        for (const QString &line : statsetsRaw) {
            if (setRe.match(line).hasMatch() && setRe.match(line).captured(1).toInt() == i + 1) {
                statsets.append(cur);
                statsetOffsets.append(cur.size() + statsetOffsets[i]);
                cur.clear(); ++i;
                continue;
            }
            auto me = entryRe.match(line);
            if (me.hasMatch()) {
                for (int g = 1; g <= 5; ++g)
                    cur.append(static_cast<char>(me.captured(g).toInt()));
            }
        }
        statsets.append(cur);
    }

    // ── Parse itemsets.txt ─────────────────────────────────────────────────────
    QVector<QByteArray> itemsets;
    QVector<int> itemsetOffsets = {0, 1};
    {
        QByteArray cur;
        int i = 1;
        QRegularExpression setRe(QStringLiteral("^Itemset (\\d+):$"));
        QRegularExpression entryRe(QStringLiteral("^(\\d+) (\\d+) (.*?)(\\s*\\[.*?\\]|$)"));
        for (const QString &line : itemsetsRaw) {
            if (setRe.match(line).hasMatch() && setRe.match(line).captured(1).toInt() == i + 1) {
                itemsets.append(cur);
                itemsetOffsets.append(2 + cur.size() + itemsetOffsets[i]);
                cur.clear(); ++i;
                continue;
            }
            auto me = entryRe.match(line);
            if (me.hasMatch()) {
                int minLv   = me.captured(1).toInt();
                int maxLv   = me.captured(2).toInt();
                QString name = me.captured(3).trimmed();
                int ii = items.indexOf(name);
                cur.append(static_cast<char>(minLv));
                cur.append(static_cast<char>(maxLv));
                cur.append(static_cast<char>(ii >> 8));
                cur.append(static_cast<char>(ii & 0xff));
            }
        }
        itemsets.append(cur);
    }

    // ── Parse schools.txt ──────────────────────────────────────────────────────
    QStringList schoolNames;
    QString schoolsHeader;
    {
        auto m = QRegularExpression(QStringLiteral(": (.*?)$")).match(schoolsRaw.isEmpty() ? QString() : schoolsRaw[0]);
        schoolsHeader = m.hasMatch() ? m.captured(1) : QStringLiteral("0");
        QRegularExpression re(QStringLiteral("\\d+: (.*?)$"));
        for (const QString &line : schoolsRaw) {
            auto ms = re.match(line);
            if (ms.hasMatch()) schoolNames.append(ms.captured(1));
        }
        if (!schoolNames.isEmpty()) schoolNames.removeFirst();
    }
    QVector<int> schoolOffsets = {0, 1};
    for (int i = 0; i < schoolNames.size(); ++i)
        schoolOffsets.append(i + schoolNames[i].length() + 2);

    // Append skill/stat/item/school offset bytes to unitRaw
    for (int i = 0; i < numEntries; ++i) {
        auto appendU16BE = [&](int v) {
            unitRaw[i].append(static_cast<char>(v >> 8));
            unitRaw[i].append(static_cast<char>(v & 0xff));
        };
        appendU16BE(skillsetOffsets[unitSkillsets[i].toInt()]);
        appendU16BE(statsetOffsets[unitStatsets[i].toInt()]);
        appendU16BE(itemsetOffsets[unitItemsets[i].toInt()]);
        appendU16BE(schoolOffsets[unitSchools[i].toInt()]);
    }

    // ── Write IDX files ────────────────────────────────────────────────────────

    // unitnames.idx
    {
        QFile f(units + "unitnames.idx"); f.open(QIODevice::WriteOnly);
        f.write("\xCD", 1);
        for (const QString &n : unitNames) { f.write(n.toUtf8()); f.write("\x00", 1); }
    }

    // unitschools.idx
    {
        QFile f(units + "unitschools.idx"); f.open(QIODevice::WriteOnly);
        quint8 hdr = static_cast<quint8>(schoolsHeader.toInt());
        f.write(reinterpret_cast<const char *>(&hdr), 1);
        for (const QString &s : schoolNames) { f.write(s.toUtf8()); f.write("\x00", 1); }
    }

    // unittints.idx
    {
        QFile f(units + "unittints.idx"); f.open(QIODevice::WriteOnly);
        quint8 hdr = static_cast<quint8>(tintsHeader.toInt());
        f.write(reinterpret_cast<const char *>(&hdr), 1);
        for (const QString &v : tintComponents) {
            quint8 b = static_cast<quint8>(v.toInt());
            f.write(reinterpret_cast<const char *>(&b), 1);
        }
    }

    // unitskills.idx
    {
        QFile f(units + "unitskills.idx"); f.open(QIODevice::WriteOnly);
        f.write("\xCD", 1);
        for (const QByteArray &ss : skillsets) {
            f.write(ss);
            f.write("\x00\x00", 2);
        }
    }

    // unitstats.idx
    {
        QFile f(units + "unitstats.idx"); f.open(QIODevice::WriteOnly);
        f.write("\xCD", 1);
        for (const QByteArray &ss : statsets) f.write(ss);
    }

    // unititems.idx
    {
        QFile f(units + "unititems.idx"); f.open(QIODevice::WriteOnly);
        f.write("\xCD", 1);
        for (const QByteArray &is : itemsets) {
            f.write(is);
            f.write("\x00\x00", 2);
        }
    }

    // unitunits.idx
    {
        QFile f(units + "unitunits.idx"); f.open(QIODevice::WriteOnly);
        quint16 cnt = static_cast<quint16>(numEntries);
        char hi = static_cast<char>(cnt >> 8);
        char lo = static_cast<char>(cnt & 0xff);
        f.write(&hi, 1);
        f.write(&lo, 1);
        for (const QByteArray &e : unitRaw) f.write(e);
    }

    out(QStringLiteral("IDX repack complete: %1 units").arg(numEntries));
    return true;
}

} // namespace Tools
