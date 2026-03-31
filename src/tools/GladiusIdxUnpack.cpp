#include "GladiusIdxUnpack.h"
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

static QByteArray readFile(const QString &path, const LogCb &err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        err(QStringLiteral("Cannot open: ") + path);
        return {};
    }
    return f.readAll();
}

static quint16 beU16(const QByteArray &d, int off)
{
    return (static_cast<quint8>(d[off]) << 8) | static_cast<quint8>(d[off + 1]);
}

static QString readNullString(const QByteArray &d, int off)
{
    QString s;
    while (off < d.size() && d[off] != '\0')
        s += QChar(static_cast<quint8>(d[off++]));
    return s;
}

static QStringList parseTokSkills(const QString &path)
{
    QStringList skills;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return skills;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    QRegularExpression re(QStringLiteral("^SKILLCREATE: \"(.*?)\","));
    while (!in.atEnd()) {
        auto m = re.match(in.readLine());
        if (m.hasMatch()) skills.append(m.captured(1));
    }
    return skills;
}

static QStringList parseTokItems(const QString &path, QStringList &types)
{
    QStringList items;
    types.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return items;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    QRegularExpression re(QStringLiteral("^ITEMCREATE: \"(.*?)\", \"(.*?)\","));
    while (!in.atEnd()) {
        auto m = re.match(in.readLine());
        if (m.hasMatch()) { items.append(m.captured(1)); types.append(m.captured(2)); }
    }
    return items;
}

static QStringList parseTokClasses(const QString &path)
{
    QStringList classes;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return classes;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    QRegularExpression re(QStringLiteral("^CREATECLASS: (.*?)$"));
    while (!in.atEnd()) {
        auto m = re.match(in.readLine());
        if (m.hasMatch()) classes.append(m.captured(1));
    }
    return classes;
}

bool idxUnpack(const QString &dataDir, const LogCb &out, const LogCb &err)
{
    const QString units  = dataDir + "units/";
    const QString config = dataDir + "config/";
    QDir().mkpath(units);
    QDir().mkpath(dataDir + "debug/");

    // ── Read binary IDX files ──────────────────────────────────────────────────
    QByteArray unitunits  = readFile(units + "unitunits.idx",  err);
    QByteArray unitnames  = readFile(units + "unitnames.idx",  err);
    QByteArray unittints  = readFile(units + "unittints.idx",  err);
    QByteArray unitskills = readFile(units + "unitskills.idx", err);
    QByteArray unitstats  = readFile(units + "unitstats.idx",  err);
    QByteArray unititems  = readFile(units + "unititems.idx",  err);
    QByteArray unitschools= readFile(units + "unitschools.idx",err);
    if (unitunits.isEmpty()) return false;

    // ── Parse unitunits.idx ───────────────────────────────────────────────────
    int numUnits = beU16(unitunits, 0);
    out(QStringLiteral("Units: %1").arg(numUnits));
    QVector<QByteArray> entries(numUnits);
    for (int i = 0; i < numUnits; ++i)
        entries[i] = unitunits.mid(2 + i * 14, 14);

    // ── Unit names ─────────────────────────────────────────────────────────────
    QStringList unitNames;
    for (const auto &e : entries)
        unitNames.append(readNullString(unitnames, beU16(e, 0)));

    // ── Schools ───────────────────────────────────────────────────────────────
    QVector<int> unitSchoolAddresses;
    for (const auto &e : entries)
        unitSchoolAddresses.append(beU16(e, 12));

    QVector<int> schoolAddressIndex;
    {
        QVector<int> tmp = unitSchoolAddresses;
        std::sort(tmp.begin(), tmp.end());
        tmp.erase(std::unique(tmp.begin(), tmp.end()), tmp.end());
        schoolAddressIndex = tmp;
    }
    QStringList schools;
    {
        schoolAddressIndex.append(unitschools.size());
        for (int i = 0; i + 1 < schoolAddressIndex.size(); ++i)
            schools.append(readNullString(unitschools, schoolAddressIndex[i]));
        schoolAddressIndex.removeLast();
    }
    QVector<int> unitSchoolsIndex;
    for (int addr : unitSchoolAddresses) {
        int idx = schoolAddressIndex.indexOf(addr);
        unitSchoolsIndex.append(idx >= 0 ? idx : 0);
    }

    // ── Classes / Outfit / Affinity ────────────────────────────────────────────
    QStringList classes = parseTokClasses(config + "classdefs.tok");
    QStringList unitClassNames, unitAffinities;
    QVector<int> unitOutfits;
    for (const auto &e : entries) {
        int ci = static_cast<quint8>(e[2]);
        unitClassNames.append(ci < classes.size() ? classes[ci] : QStringLiteral("?"));
        quint8 byte3 = static_cast<quint8>(e[3]);
        unitOutfits.append((byte3 >> 3) & 0x1f);  // top 5 bits
        int afIdx = byte3 & 0x07;
        unitAffinities.append(afIdx < kAffinity.size() ? kAffinity[afIdx] : QStringLiteral("?"));
    }

    // ── Tints ─────────────────────────────────────────────────────────────────
    QVector<int> unitTints;
    for (const auto &e : entries) {
        int raw = beU16(e, 4);
        unitTints.append(std::max(0, (raw + 17) / 18));
    }
    // Tint records: byte 0 = header, then 18-byte records
    QVector<QByteArray> tintRecords;
    tintRecords.append(QByteArray());  // slot 0 = blank
    for (int off = 1; off + 18 <= unittints.size(); off += 18)
        tintRecords.append(unittints.mid(off, 18));

    // ── Skill sets ────────────────────────────────────────────────────────────
    QVector<int> unitSkillAddresses;
    for (const auto &e : entries)
        unitSkillAddresses.append(beU16(e, 6));

    QVector<int> skillSetAddressIndex = {1};
    QVector<QByteArray> skillSetsRaw;
    {
        QByteArray current;
        int i = 0;
        for (int pos = 1; pos + 1 < unitskills.size(); ) {
            quint8 b0 = static_cast<quint8>(unitskills[pos]);
            quint8 b1 = static_cast<quint8>(unitskills[pos + 1]);
            if (b0 == 0 && b1 == 0 && (current.isEmpty() || current.size() % 4 == 0)) {
                skillSetsRaw.append(current);
                skillSetAddressIndex.append(skillSetAddressIndex[i] + current.size() + 2);
                current.clear();
                ++i;
                pos += 2;
            } else {
                current.append(unitskills[pos]);
                current.append(unitskills[pos + 1]);
                pos += 2;
            }
        }
    }
    QVector<int> unitSkillSets;
    for (int addr : unitSkillAddresses) {
        int idx = skillSetAddressIndex.indexOf(addr);
        unitSkillSets.append(idx >= 0 ? idx : 0);
    }

    // ── Stat sets ─────────────────────────────────────────────────────────────
    QVector<int> unitStatAddresses;
    for (const auto &e : entries)
        unitStatAddresses.append(beU16(e, 8));

    QVector<int> statSetAddressIndex = {1};
    QVector<QByteArray> statSetsRaw;
    {
        int i = 0;
        for (int pos = 1; pos + 150 <= unitstats.size(); pos += 150) {
            statSetsRaw.append(unitstats.mid(pos, 150));
            statSetAddressIndex.append(statSetAddressIndex[i] + 150);
            ++i;
        }
    }
    QVector<int> unitStatSets;
    for (int addr : unitStatAddresses) {
        int idx = statSetAddressIndex.indexOf(addr);
        unitStatSets.append(idx >= 0 ? idx : 0);
    }

    // ── Item sets ─────────────────────────────────────────────────────────────
    QVector<int> unitItemAddresses;
    for (const auto &e : entries)
        unitItemAddresses.append(beU16(e, 10));

    QVector<int> itemSetAddressIndex = {0, 1};
    QVector<QByteArray> itemSetsRaw;
    itemSetsRaw.append(QByteArray());  // slot 0 = blank
    {
        QByteArray current;
        int i = 1;
        for (int pos = 1; pos + 1 < unititems.size(); ) {
            quint8 b0 = static_cast<quint8>(unititems[pos]);
            quint8 b1 = static_cast<quint8>(unititems[pos + 1]);
            if (b0 == 0 && b1 == 0 && (current.isEmpty() || current.size() % 4 == 0)) {
                itemSetsRaw.append(current);
                itemSetAddressIndex.append(itemSetAddressIndex[i] + current.size() + 2);
                current.clear();
                ++i;
                pos += 2;
            } else {
                current.append(unititems[pos]);
                current.append(unititems[pos + 1]);
                pos += 2;
            }
        }
    }
    QVector<int> unitItemSets;
    for (int addr : unitItemAddresses) {
        int idx = itemSetAddressIndex.indexOf(addr);
        unitItemSets.append(idx >= 0 ? idx : 0);
    }

    // ── Load skill/item names for text output ─────────────────────────────────
    QStringList skills = parseTokSkills(config + "skills.tok");
    QStringList itemTypes;
    QStringList items = parseTokItems(config + "items.tok", itemTypes);

    // ── Write gladiators.txt ──────────────────────────────────────────────────
    {
        QFile f(units + "gladiators.txt");
        f.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream ts(&f);
        ts.setEncoding(QStringConverter::Utf8);
        for (int i = 0; i < numUnits; ++i) {
            ts << "Name: "     << unitNames[i]      << '\n';
            ts << "Class: "    << unitClassNames[i] << '\n';
            ts << "Outfit: "   << unitOutfits[i]    << '\n';
            ts << "Affinity: " << unitAffinities[i] << '\n';
            ts << "Tint set: " << unitTints[i]      << '\n';
            ts << "Skill set: "<< unitSkillSets[i]  << '\n';
            ts << "Stat set: " << unitStatSets[i]   << '\n';
            ts << "Item set: " << unitItemSets[i]   << '\n';
            ts << "School: "   << unitSchoolsIndex[i] << '\n';
            ts << '\n';
        }
    }

    // ── Write schools.txt ──────────────────────────────────────────────────────
    {
        QFile f(units + "schools.txt");
        f.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream ts(&f);
        ts.setEncoding(QStringConverter::Utf8);
        quint8 schoolHdr = static_cast<quint8>(unitschools.isEmpty() ? 0 : unitschools[0]);
        ts << "Header (IDK what this means; change it if you want to experiment): "
           << schoolHdr << "\n\n";
        ts << "NUMENTRIES: " << schools.size() << "\n\n";
        for (int i = 0; i < schools.size(); ++i)
            ts << i << ": " << schools[i] << '\n';
    }

    // ── Write tints.txt ───────────────────────────────────────────────────────
    {
        QFile f(units + "tints.txt");
        f.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream ts(&f);
        ts.setEncoding(QStringConverter::Utf8);
        quint8 tintHdr = static_cast<quint8>(unittints.isEmpty() ? 0 : unittints[0]);
        ts << "Header (IDK what this means; change it if you want to experiment): "
           << tintHdr << "\n\n";
        ts << "NUMENTRIES: " << tintRecords.size() << "\n\n";
        ts << "Tint set 0:\nThis tint set is blank; it is unknown what it means for a unit to use tint set 0 "
              "(possibly default colours?). Needs more research.\n\n";
        for (int i = 1; i < tintRecords.size(); ++i) {
            const QByteArray &rec = tintRecords[i];
            auto byte = [&](int j) { return static_cast<int>(static_cast<quint8>(rec[j])); };
            ts << "Tint set " << i << ":\n";
            ts << "Cloth1: " << byte(0)  << ' ' << byte(1)  << ' ' << byte(2)  << '\n';
            ts << "Cloth2: " << byte(3)  << ' ' << byte(4)  << ' ' << byte(5)  << '\n';
            ts << "Armor1: " << byte(6)  << ' ' << byte(7)  << ' ' << byte(8)  << '\n';
            ts << "Armor2: " << byte(9)  << ' ' << byte(10) << ' ' << byte(11) << '\n';
            ts << "Skin: "  << byte(12) << ' ' << byte(13) << ' ' << byte(14) << '\n';
            ts << "Hair: "  << byte(15) << ' ' << byte(16) << ' ' << byte(17) << "\n\n";
        }
    }

    // ── Write skillsets.txt ───────────────────────────────────────────────────
    {
        QFile f(units + "skillsets.txt");
        f.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream ts(&f);
        ts.setEncoding(QStringConverter::Utf8);
        ts << "NUMENTRIES: " << skillSetsRaw.size() << "\n\n";
        ts << "Legend:\nMinLevelLearned MaxLevelLearned SkillName\n\n";
        for (int i = 0; i < skillSetsRaw.size(); ++i) {
            ts << "Skillset " << i << ":\n";
            const QByteArray &ss = skillSetsRaw[i];
            for (int j = 0; j + 3 < ss.size(); j += 4) {
                int minLv  = static_cast<quint8>(ss[j]);
                int maxLv  = static_cast<quint8>(ss[j + 1]);
                int skillIdx = (static_cast<quint8>(ss[j + 2]) << 8) | static_cast<quint8>(ss[j + 3]);
                QString skillName = (skillIdx < skills.size()) ? skills[skillIdx] : QStringLiteral("?");
                ts << minLv << ' ' << maxLv << ' ' << skillName << '\n';
            }
            ts << '\n';
        }
    }

    // ── Write statsets.txt ────────────────────────────────────────────────────
    {
        QFile f(units + "statsets.txt");
        f.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream ts(&f);
        ts.setEncoding(QStringConverter::Utf8);
        ts << "NUMENTRIES: " << statSetsRaw.size() << '\n';
        ts << "Legend:\nLevel: Con Pow Acc Def Ini/Mov\n\n";
        for (int i = 0; i < statSetsRaw.size(); ++i) {
            ts << "Statset " << i << ":\n";
            const QByteArray &ss = statSetsRaw[i];
            for (int j = 0, lv = 1; j + 4 < ss.size(); j += 5, ++lv) {
                ts << lv << ": "
                   << static_cast<quint8>(ss[j])   << ' '
                   << static_cast<quint8>(ss[j+1]) << ' '
                   << static_cast<quint8>(ss[j+2]) << ' '
                   << static_cast<quint8>(ss[j+3]) << ' '
                   << static_cast<quint8>(ss[j+4]) << '\n';
            }
            ts << '\n';
        }
    }

    // ── Write itemsets.txt ────────────────────────────────────────────────────
    {
        QFile f(units + "itemsets.txt");
        f.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream ts(&f);
        ts.setEncoding(QStringConverter::Utf8);
        ts << "NUMENTRIES: " << itemSetsRaw.size() << "\n\n";
        ts << "Legend:\nMinLevelUsed MaxLevelUsed ItemName  "
              "[Any text here is ignored; use this space for notes. "
              "If you don't want to use it, delete the square brackets and remove any whitespace after the item name.]\n\n";
        for (int i = 0; i < itemSetsRaw.size(); ++i) {
            ts << "Itemset " << i << ":\n";
            const QByteArray &is = itemSetsRaw[i];
            for (int j = 0; j + 3 < is.size(); j += 4) {
                int minLv   = static_cast<quint8>(is[j]);
                int maxLv   = static_cast<quint8>(is[j + 1]);
                int itemIdx = (static_cast<quint8>(is[j + 2]) << 8) | static_cast<quint8>(is[j + 3]);
                QString itemName = (itemIdx < items.size()) ? items[itemIdx] : QStringLiteral("?");
                QString itemType = (itemIdx < itemTypes.size()) ? itemTypes[itemIdx] : QStringLiteral("?");
                ts << minLv << ' ' << maxLv << ' ' << itemName << " [" << itemType << "]\n";
            }
            ts << '\n';
        }
    }

    out(QStringLiteral("IDX unpack complete: %1 units").arg(numUnits));
    return true;
}

} // namespace Tools
