#include "SchoolBuilderTab.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QScrollArea>
#include <QFrame>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QDialog>
#include <QMessageBox>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <algorithm>

// ── Local helpers ─────────────────────────────────────────────────────────────

static QString unquote(QString s)
{
    s = s.trimmed();
    if (s.startsWith('"')) s.remove(0, 1);
    if (s.endsWith('"'))   s.chop(1);
    return s;
}

static QVector<int> parseIntList(const QString &s)
{
    QVector<int> v;
    for (const QString &p : s.split(',')) {
        bool ok;
        int n = p.trimmed().toInt(&ok);
        if (ok) v.append(n);
    }
    return v;
}

// Split a string by commas, respecting double-quoted sections.
static QStringList splitQuoted(const QString &s)
{
    QStringList parts;
    QString cur;
    bool inQ = false;
    for (QChar c : s) {
        if (c == '"') { inQ = !inQ; cur += c; }
        else if (c == ',' && !inQ) { parts.append(cur.trimmed()); cur.clear(); }
        else cur += c;
    }
    if (!cur.trimmed().isEmpty()) parts.append(cur.trimmed());
    return parts;
}

// static
QString SchoolBuilderTab::resourcesDir()
{
    return QCoreApplication::applicationDirPath() + "/resources/";
}

// ── Constructor ───────────────────────────────────────────────────────────────

SchoolBuilderTab::SchoolBuilderTab(QWidget *parent)
    : QWidget(parent)
{
    // ── School selector ───────────────────────────────────────────────────────
    m_valensRadio = new QRadioButton("Valens Imperia (valensimperia.tok)", this);
    m_ursulaRadio = new QRadioButton("Ursula Nordagh (ursulanordagh.tok)", this);
    m_valensRadio->setChecked(true);

    auto *schoolGroup = new QButtonGroup(this);
    schoolGroup->addButton(m_valensRadio);
    schoolGroup->addButton(m_ursulaRadio);
    connect(m_valensRadio, &QRadioButton::toggled, this, &SchoolBuilderTab::onSchoolChanged);

    m_loadDataBtn = new QPushButton("Load data…", this);
    m_loadDataBtn->setToolTip(
        "Load class, item, and skill tables from a previously saved JSON file.\n"
        "Use this when maintaining multiple modded working directories, each with\n"
        "its own schoolbuilder JSON generated from that mod's BEC.");
    connect(m_loadDataBtn, &QPushButton::clicked, this, &SchoolBuilderTab::onLoadData);

    m_dataSourceLabel = new QLabel(this);
    m_dataSourceLabel->setStyleSheet("color: gray; font-style: italic;");

    m_generateJsonBtn = new QPushButton("Generate data JSON…", this);
    m_generateJsonBtn->setToolTip(
        "Parse classdefs.tok, items.tok, and skills.tok from an unpacked BEC and save\n"
        "the result as a named JSON file in the resources/ folder.\n\n"
        "The vanilla data is already bundled with the tool. Use this only if you are\n"
        "working with a modded version of the game and want a separate data file for it.");
    connect(m_generateJsonBtn, &QPushButton::clicked, this, &SchoolBuilderTab::onGenerateJson);

    m_saveBtn = new QPushButton("Save School File", this);
    m_saveBtn->setEnabled(false);
    connect(m_saveBtn, &QPushButton::clicked, this, &SchoolBuilderTab::onSaveSchool);

    m_bypassCheckbox = new QCheckBox("Bypass vanilla restrictions", this);
    m_bypassCheckbox->setToolTip(
        "<b>Bypass vanilla restrictions</b><br>"
        "When checked:<br>"
        "&nbsp;&nbsp;• Stats and Job Points become manually editable<br>"
        "&nbsp;&nbsp;• Item level requirements are removed (class restrictions still apply)<br>"
        "<br>"
        "Useful for creating custom gladiators that go beyond the original game's limits.");
    connect(m_bypassCheckbox, &QCheckBox::toggled,
            this, &SchoolBuilderTab::onBypassToggled);

    // Row 1: school selector + bypass + save
    auto *topBar = new QHBoxLayout;
    topBar->addWidget(new QLabel("School:"));
    topBar->addWidget(m_valensRadio);
    topBar->addWidget(m_ursulaRadio);
    topBar->addStretch();
    topBar->addWidget(m_bypassCheckbox);
    topBar->addSpacing(12);
    topBar->addWidget(m_saveBtn);

    // Row 2: active data source + load + generate
    auto *dataBar = new QHBoxLayout;
    dataBar->addWidget(new QLabel("Data:"));
    dataBar->addWidget(m_dataSourceLabel, 1);
    dataBar->addWidget(m_loadDataBtn);
    dataBar->addSpacing(4);
    dataBar->addWidget(m_generateJsonBtn);

    // ── Hint label ────────────────────────────────────────────────────────────
    m_hintLabel = new QLabel(this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setWordWrap(true);

    // ── Left panel: current gladiators ────────────────────────────────────────
    m_gladiatorList = new QListWidget(this);
    m_gladiatorList->setMinimumWidth(220);

    m_editBtn   = new QPushButton("Edit",   this);
    m_removeBtn = new QPushButton("Remove", this);
    m_editBtn->setEnabled(false);
    m_removeBtn->setEnabled(false);

    connect(m_gladiatorList, &QListWidget::currentRowChanged,
            this, &SchoolBuilderTab::onGladiatorSelectionChanged);
    connect(m_editBtn,   &QPushButton::clicked, this, &SchoolBuilderTab::editSelectedGladiator);
    connect(m_removeBtn, &QPushButton::clicked, this, &SchoolBuilderTab::removeSelectedGladiator);

    auto *listBtnRow = new QHBoxLayout;
    listBtnRow->addWidget(m_editBtn);
    listBtnRow->addWidget(m_removeBtn);

    auto *leftGroup = new QGroupBox("Current School", this);
    auto *leftLayout = new QVBoxLayout(leftGroup);
    leftLayout->addWidget(m_gladiatorList);
    leftLayout->addLayout(listBtnRow);

    // ── Right panel: add gladiator form ───────────────────────────────────────
    m_nameEdit  = new QLineEdit(this);
    m_classCombo = new QComboBox(this);
    m_classCombo->setMinimumWidth(180);

    m_levelSpin = new QSpinBox(this);
    m_levelSpin->setRange(1, 20);
    m_levelSpin->setValue(1);

    m_statsEdit = new QLineEdit("0, 0, 0, 0, 0, 0", this);
    m_statsEdit->setReadOnly(true);
    m_statsEdit->setStyleSheet("QLineEdit[readOnly=\"true\"] { background: palette(window); }");

    m_jpSpin = new QSpinBox(this);
    m_jpSpin->setRange(0, 9999);
    m_jpSpin->setValue(5);
    m_jpSpin->setEnabled(false);

    m_weaponCombo    = new QComboBox(this);
    m_armorCombo     = new QComboBox(this);
    m_shieldCombo    = new QComboBox(this);
    m_helmetCombo    = new QComboBox(this);
    m_accessoryCombo = new QComboBox(this);
    for (auto *c : {m_weaponCombo, m_armorCombo, m_shieldCombo,
                    m_helmetCombo, m_accessoryCombo})
        c->setMinimumWidth(200);

    m_skillList = new QListWidget(this);
    m_skillList->setMinimumHeight(160);

    m_addBtn        = new QPushButton("Add to School", this);
    m_cancelEditBtn = new QPushButton("Cancel Edit",  this);
    m_cancelEditBtn->hide();

    connect(m_classCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SchoolBuilderTab::onClassChanged);
    connect(m_levelSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SchoolBuilderTab::onLevelChanged);
    connect(m_addBtn,        &QPushButton::clicked, this, &SchoolBuilderTab::addGladiator);
    connect(m_cancelEditBtn, &QPushButton::clicked, this, &SchoolBuilderTab::clearEditMode);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->addRow("Name:",  m_nameEdit);
    form->addRow("Class:", m_classCombo);
    form->addRow("Level:", m_levelSpin);
    form->addRow("Stats (CON, PWR, ACC, DEF, INT, MOVE):", m_statsEdit);
    form->addRow("Job Points:", m_jpSpin);
    form->addRow("Weapon:",     m_weaponCombo);
    form->addRow("Armor:",      m_armorCombo);
    form->addRow("Shield:",     m_shieldCombo);
    form->addRow("Helmet:",     m_helmetCombo);
    form->addRow("Accessory:",  m_accessoryCombo);

    auto *skillsGroup = new QGroupBox("Starting Skills", this);
    auto *skillsLayout = new QVBoxLayout(skillsGroup);
    skillsLayout->addWidget(m_skillList);

    auto *addBtnRow = new QHBoxLayout;
    addBtnRow->addWidget(m_addBtn);
    addBtnRow->addWidget(m_cancelEditBtn);

    auto *formContainer = new QWidget;
    auto *formLayout    = new QVBoxLayout(formContainer);
    formLayout->addLayout(form);
    formLayout->addWidget(skillsGroup);
    formLayout->addLayout(addBtnRow);
    formLayout->addStretch();

    auto *formScroll = new QScrollArea(this);
    formScroll->setWidget(formContainer);
    formScroll->setWidgetResizable(true);
    formScroll->setFrameShape(QFrame::NoFrame);

    m_rightGroup = new QGroupBox("Add Gladiator", this);
    auto *rightLayout = new QVBoxLayout(m_rightGroup);
    rightLayout->addWidget(formScroll);

    // ── Splitter ──────────────────────────────────────────────────────────────
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftGroup);
    splitter->addWidget(m_rightGroup);
    splitter->setStretchFactor(1, 2);

    m_mainWidget = splitter;
    m_mainWidget->hide();

    // ── Main layout ───────────────────────────────────────────────────────────
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addLayout(topBar);
    layout->addLayout(dataBar);
    layout->addWidget(m_hintLabel, 1);
    layout->addWidget(m_mainWidget, 1);

    // ── Try to load bundled JSON at startup ───────────────────────────────────
    tryLoadBundledJson();
    updateHintLabel();
}

// ── Public API ────────────────────────────────────────────────────────────────

void SchoolBuilderTab::setRootPath(const QString &moddedDir, const QString &)
{
    m_moddedDir = moddedDir;

    if (!m_dataLoaded) {
        updateHintLabel();
        return;
    }

    loadSchoolFile();
    refreshGladiatorList();
    refreshStats();
    refreshItemCombos();
    refreshSkillList();

    updateHintLabel();
}

// ── Private slots ─────────────────────────────────────────────────────────────

void SchoolBuilderTab::onSchoolChanged()
{
    if (m_moddedDir.isEmpty()) return;
    loadSchoolFile();
    refreshGladiatorList();
}

void SchoolBuilderTab::onClassChanged(int)
{
    refreshStats();
    refreshItemCombos();
    refreshSkillList();
}

void SchoolBuilderTab::onLevelChanged(int)
{
    if (!isBypassMode()) refreshStats();
    refreshItemCombos();
    refreshSkillList();
}

void SchoolBuilderTab::addGladiator()
{
    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Missing name", "Please enter a gladiator name.");
        return;
    }
    QString className = m_classCombo->currentText();
    if (!m_classes.contains(className)) return;

    const ClassDef &cls = m_classes[className];
    int level = m_levelSpin->value();

    GladiatorEntry g;
    g.name       = name;
    g.className  = className;
    g.level      = level;
    g.experience = 0;
    g.isRequired = false;

    if (isBypassMode()) {
        g.coreStats = parseIntList(m_statsEdit->text());
        while (g.coreStats.size() < 6) g.coreStats.append(0);
        g.jobPoints = m_jpSpin->value();
    } else {
        g.coreStats = computeStats(cls, level);
        g.jobPoints = computeJP(cls, level);
    }

    auto comboVal = [](QComboBox *c) -> QString {
        QString v = c->currentText();
        return (v == "(none)") ? "" : v;
    };
    g.items = {
        comboVal(m_weaponCombo),
        comboVal(m_armorCombo),
        comboVal(m_shieldCombo),
        comboVal(m_helmetCombo),
        comboVal(m_accessoryCombo)
    };

    for (int i = 0; i < m_skillList->count(); ++i) {
        auto *item = m_skillList->item(i);
        if (item->checkState() == Qt::Checked)
            g.skills.append(item->text());
    }

    if (m_editingIndex >= 0 && m_editingIndex < m_gladiators.size()) {
        // Preserve isRequired flag of the entry being replaced
        g.isRequired = m_gladiators[m_editingIndex].isRequired;
        m_gladiators[m_editingIndex] = g;
        clearEditMode();
    } else {
        m_gladiators.append(g);
        m_nameEdit->clear();
        // Uncheck all skills for next entry
        for (int i = 0; i < m_skillList->count(); ++i)
            m_skillList->item(i)->setCheckState(Qt::Unchecked);
    }
    refreshGladiatorList();
}

void SchoolBuilderTab::onBypassToggled(bool checked)
{
    m_statsEdit->setReadOnly(!checked);
    m_statsEdit->setStyleSheet(checked
        ? ""
        : "QLineEdit[readOnly=\"true\"] { background: palette(window); }");
    m_jpSpin->setEnabled(checked);

    if (!checked) {
        // Turning bypass off — recompute stats/JP from current class & level
        refreshStats();
    }
    // Always refresh items: bypass removes level filter
    refreshItemCombos();
}

void SchoolBuilderTab::onSaveSchool()
{
    QString schoolDir = resolvePath(m_moddedDir, "data", "school");
    QString path      = schoolDir + schoolFileName();

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Save failed", "Cannot write:\n" + path);
        return;
    }

    QTextStream ts(&f);
    ts << m_schoolHeader;

    for (const GladiatorEntry &g : m_gladiators)
        ts << "\n" << formatGladiator(g);

    QMessageBox::information(this, "Saved",
        "School file saved:\n" + QFileInfo(path).fileName());
}

void SchoolBuilderTab::onLoadData()
{
    QString start = m_activeJsonPath.isEmpty() ? resourcesDir() : m_activeJsonPath;
    QString path = QFileDialog::getOpenFileName(
        this, "Load School Builder data file", start,
        "School Builder data (*.json);;All files (*)");
    if (path.isEmpty()) return;

    if (!loadFromJson(path)) {
        QMessageBox::critical(this, "Load failed",
            "Could not read a valid School Builder data file from:\n" + path);
        return;
    }
    applyLoadedData(path);

    if (!m_moddedDir.isEmpty()) {
        loadSchoolFile();
        refreshGladiatorList();
        refreshStats();
        refreshItemCombos();
        refreshSkillList();
    }
    updateHintLabel();
}

void SchoolBuilderTab::onGenerateJson()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Generate School Builder data from BEC");
    dlg.setMinimumWidth(580);

    auto *infoLabel = new QLabel(
        "<b>Vanilla data is bundled with the tool.</b> Only use this when working with "
        "a mod that changes classdefs.tok, items.tok, or skills.tok and you want the "
        "School Builder to reflect those changes.<br><br>"
        "Select the root of an unpacked BEC (e.g. your working_BEC folder) and give "
        "the output file a descriptive name. The file is saved to the <tt>resources/</tt> "
        "folder and can be reloaded later with <b>Load data…</b>.",
        &dlg);
    infoLabel->setWordWrap(true);
    infoLabel->setTextFormat(Qt::RichText);

    // BEC source directory — default to working_BEC if set
    auto *dirEdit   = new QLineEdit(m_moddedDir, &dlg);
    auto *browseBtn = new QPushButton("Browse…", &dlg);

    // Output filename — default to a unique name based on the BEC folder name
    QString defaultName = m_moddedDir.isEmpty()
        ? "schoolbuilder_vanilla.json"
        : QFileInfo(m_moddedDir).fileName().replace(' ', '_') + "_schoolbuilder.json";
    auto *outNameEdit = new QLineEdit(defaultName, &dlg);
    outNameEdit->setPlaceholderText("e.g. my_mod_schoolbuilder.json");

    auto *genBtn      = new QPushButton("Generate", &dlg);
    auto *statusLabel = new QLabel(&dlg);
    statusLabel->setWordWrap(true);

    auto *dirRow = new QHBoxLayout;
    dirRow->addWidget(new QLabel("BEC directory:", &dlg));
    dirRow->addWidget(dirEdit, 1);
    dirRow->addWidget(browseBtn);

    auto *outRow = new QHBoxLayout;
    outRow->addWidget(new QLabel("Output filename:", &dlg));
    outRow->addWidget(outNameEdit, 1);
    outRow->addWidget(new QLabel("(saved to resources/)", &dlg));

    auto *layout = new QVBoxLayout(&dlg);
    layout->addWidget(infoLabel);
    layout->addSpacing(8);
    layout->addLayout(dirRow);
    layout->addSpacing(4);
    layout->addLayout(outRow);
    layout->addSpacing(4);
    layout->addWidget(genBtn);
    layout->addWidget(statusLabel);
    layout->addStretch();

    connect(browseBtn, &QPushButton::clicked, [&]() {
        QString start = dirEdit->text().isEmpty() ? QDir::homePath() : dirEdit->text();
        QString dir = QFileDialog::getExistingDirectory(
            &dlg, "Select unpacked BEC root directory", start);
        if (!dir.isEmpty()) {
            dirEdit->setText(dir);
            // Auto-suggest an output name from the folder if the user hasn't typed one
            if (outNameEdit->text() == defaultName || outNameEdit->text().isEmpty()) {
                QString suggested = QFileInfo(dir).fileName().replace(' ', '_')
                                    + "_schoolbuilder.json";
                outNameEdit->setText(suggested);
            }
        }
    });

    connect(genBtn, &QPushButton::clicked, [&]() {
        QString becDir   = dirEdit->text().trimmed();
        QString outName  = outNameEdit->text().trimmed();

        if (becDir.isEmpty()) {
            statusLabel->setText("Please select a BEC directory first.");
            return;
        }
        if (outName.isEmpty()) {
            statusLabel->setText("Please enter an output filename.");
            return;
        }
        if (!outName.endsWith(".json", Qt::CaseInsensitive))
            outName += ".json";

        QString cfgDir    = resolvePath(becDir, "data", "config");
        QString classFile = cfgDir + "classdefs.tok";
        QString itemsFile = cfgDir + "items.tok";
        QString skillsFile = cfgDir + "skills.tok";

        if (!QFile::exists(classFile)) {
            statusLabel->setText("classdefs.tok not found in:\n" + cfgDir
                + "\n\nMake sure you selected the root of an unpacked BEC.");
            return;
        }

        m_classes.clear();
        m_items.clear();
        m_skills.clear();
        parseClassDefs(classFile);
        parseItems(itemsFile);
        parseSkills(skillsFile);

        QString outPath = resourcesDir() + outName;
        if (!serializeToJson(outPath)) {
            statusLabel->setText("Failed to write JSON to:\n" + outPath);
            return;
        }

        applyLoadedData(outPath);

        // If a modded dir is already set, refresh the full UI
        if (!m_moddedDir.isEmpty()) {
            loadSchoolFile();
            refreshGladiatorList();
            refreshStats();
            refreshItemCombos();
            refreshSkillList();
        }
        updateHintLabel();

        statusLabel->setText(
            QString("Generated: %1 classes, %2 items, %3 skills\nSaved to: %4")
            .arg(m_classes.size()).arg(m_items.size()).arg(m_skills.size())
            .arg(outPath));
        genBtn->setEnabled(false);
    });

    dlg.exec();
}

// ── JSON persistence ──────────────────────────────────────────────────────────

void SchoolBuilderTab::tryLoadBundledJson()
{
    QString path = resourcesDir() + "schoolbuilder_vanilla.json";
    if (loadFromJson(path))
        applyLoadedData(path);
}

void SchoolBuilderTab::applyLoadedData(const QString &path)
{
    m_dataLoaded = true;
    m_activeJsonPath = path;
    m_dataSourceLabel->setText(QFileInfo(path).fileName());
    m_dataSourceLabel->setToolTip(path);
    populateClassCombo();
}

bool SchoolBuilderTab::loadFromJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseErr);
    if (doc.isNull() || !doc.isObject()) return false;

    QJsonObject root = doc.object();

    // Classes
    m_classes.clear();
    QJsonObject classesObj = root["classes"].toObject();
    for (auto it = classesObj.begin(); it != classesObj.end(); ++it) {
        ClassDef cls;
        cls.name = it.key();
        QJsonObject obj = it.value().toObject();
        cls.skillUseName = obj["skillUseName"].toString();

        for (auto v : obj["levelZeroStats"].toArray())
            cls.levelZeroStats.append(v.toInt());

        for (auto row : obj["levelStatAwards"].toArray()) {
            QVector<int> rowVec;
            for (auto v : row.toArray())
                rowVec.append(v.toInt());
            cls.levelStatAwards.append(rowVec);
        }

        for (auto v : obj["levelUpJpGiven"].toArray())
            cls.levelUpJpGiven.append(v.toInt());

        for (auto cat : obj["itemCats"].toArray()) {
            QJsonObject catObj = cat.toObject();
            ItemCatEntry entry;
            entry.slot     = catObj["slot"].toString();
            entry.subtype  = catObj["subtype"].toString();
            entry.category = catObj["category"].toString();
            cls.itemCats.append(entry);
        }

        m_classes[cls.name] = cls;
    }

    // Items
    m_items.clear();
    for (auto item : root["items"].toArray()) {
        QJsonObject obj = item.toObject();
        ItemDef def;
        def.name     = obj["name"].toString();
        def.type     = obj["type"].toString();
        def.subtype  = obj["subtype"].toString();
        def.category = obj["category"].toString();
        def.minLevel = obj["minLevel"].toInt(1);
        m_items.append(def);
    }

    // Skills
    m_skills.clear();
    for (auto skill : root["skills"].toArray()) {
        QJsonObject obj = skill.toObject();
        SkillDef def;
        def.name       = obj["name"].toString();
        def.useClass   = obj["useClass"].toString();
        def.skillLevel = obj["skillLevel"].toInt();
        def.jpCost     = obj["jpCost"].toInt();
        m_skills.append(def);
    }

    return !m_classes.isEmpty();
}

bool SchoolBuilderTab::serializeToJson(const QString &path) const
{
    QJsonObject root;
    root["version"] = 1;

    // Classes
    QJsonObject classesObj;
    for (auto it = m_classes.begin(); it != m_classes.end(); ++it) {
        const ClassDef &cls = it.value();
        QJsonObject obj;
        obj["skillUseName"] = cls.skillUseName;

        QJsonArray lzs;
        for (int v : cls.levelZeroStats) lzs.append(v);
        obj["levelZeroStats"] = lzs;

        QJsonArray lsa;
        for (const QVector<int> &row : cls.levelStatAwards) {
            QJsonArray rowArr;
            for (int v : row) rowArr.append(v);
            lsa.append(rowArr);
        }
        obj["levelStatAwards"] = lsa;

        QJsonArray lpj;
        for (int v : cls.levelUpJpGiven) lpj.append(v);
        obj["levelUpJpGiven"] = lpj;

        QJsonArray cats;
        for (const ItemCatEntry &cat : cls.itemCats) {
            QJsonObject catObj;
            catObj["slot"]     = cat.slot;
            catObj["subtype"]  = cat.subtype;
            catObj["category"] = cat.category;
            cats.append(catObj);
        }
        obj["itemCats"] = cats;

        classesObj[cls.name] = obj;
    }
    root["classes"] = classesObj;

    // Items
    QJsonArray itemsArr;
    for (const ItemDef &item : m_items) {
        QJsonObject obj;
        obj["name"]     = item.name;
        obj["type"]     = item.type;
        obj["subtype"]  = item.subtype;
        obj["category"] = item.category;
        obj["minLevel"] = item.minLevel;
        itemsArr.append(obj);
    }
    root["items"] = itemsArr;

    // Skills
    QJsonArray skillsArr;
    for (const SkillDef &sk : m_skills) {
        QJsonObject obj;
        obj["name"]       = sk.name;
        obj["useClass"]   = sk.useClass;
        obj["skillLevel"] = sk.skillLevel;
        obj["jpCost"]     = sk.jpCost;
        skillsArr.append(obj);
    }
    root["skills"] = skillsArr;

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

// ── UI helpers ────────────────────────────────────────────────────────────────

void SchoolBuilderTab::populateClassCombo()
{
    m_classCombo->blockSignals(true);
    m_classCombo->clear();
    QStringList names = m_classes.keys();
    names.sort(Qt::CaseInsensitive);
    m_classCombo->addItems(names);
    m_classCombo->blockSignals(false);
}

void SchoolBuilderTab::updateHintLabel()
{
    if (!m_dataLoaded) {
        m_hintLabel->setText(
            "Vanilla data file not found (resources/schoolbuilder_vanilla.json).\n\n"
            "Click \"Regenerate data from BEC…\" and select the root of an unpacked BEC\n"
            "to rebuild it.");

        m_hintLabel->show();
        m_mainWidget->hide();
        m_saveBtn->setEnabled(false);
        return;
    }
    if (m_moddedDir.isEmpty()) {
        m_hintLabel->setText(
            "Unpack a vanilla ISO first.\n"
            "The School Builder will be available once the working_BEC is populated.");
        m_hintLabel->show();
        m_mainWidget->hide();
        m_saveBtn->setEnabled(false);
        return;
    }
    m_hintLabel->hide();
    m_mainWidget->show();
    m_saveBtn->setEnabled(true);
}

// ── Parsers ───────────────────────────────────────────────────────────────────

void SchoolBuilderTab::parseClassDefs(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream ts(&f);

    ClassDef current;
    bool inClass = false;

    auto saveClass = [&]() {
        if (inClass && !current.name.isEmpty())
            m_classes[current.name] = current;
    };

    QString line;
    while (ts.readLineInto(&line)) {
        QString t = line.trimmed();
        if (t.isEmpty() || t.startsWith("//")) continue;
        // Strip inline comments
        int ci = t.indexOf("//");
        if (ci > 0) t = t.left(ci).trimmed();

        if (t.startsWith("CREATECLASS:")) {
            saveClass();
            current = ClassDef{};
            current.name = t.mid(12).trimmed();
            inClass = true;
            continue;
        }
        if (!inClass) continue;

        if (t.startsWith("SKILLUSENAME:")) {
            current.skillUseName = unquote(t.mid(13));
        } else if (t.startsWith("ITEMCAT:")) {
            QStringList parts = t.mid(8).trimmed().split(',');
            if (parts.size() >= 3) {
                ItemCatEntry cat;
                cat.slot     = parts[0].trimmed().toLower();
                cat.subtype  = parts[1].trimmed();
                cat.category = parts[2].trimmed();
                current.itemCats.append(cat);
            }
        } else if (t.startsWith("LEVELZEROSTATS:")) {
            current.levelZeroStats = parseIntList(t.mid(15));
        } else if (t.startsWith("LEVELSTATAWARDS:")) {
            current.levelStatAwards.append(parseIntList(t.mid(16)));
        } else if (t.startsWith("LEVELUPJPGIVEN:")) {
            current.levelUpJpGiven = parseIntList(t.mid(15));
        }
    }
    saveClass();
}

void SchoolBuilderTab::parseItems(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream ts(&f);

    ItemDef current;
    bool inItem = false;

    auto saveItem = [&]() {
        if (inItem && !current.name.isEmpty())
            m_items.append(current);
    };

    QString line;
    while (ts.readLineInto(&line)) {
        QString t = line.trimmed();
        if (t.isEmpty() || t.startsWith("//")) continue;

        if (t.startsWith("ITEMCREATE:")) {
            saveItem();
            current = ItemDef{};
            // "Name", "Type", "Subtype", "Category", N
            QStringList parts = splitQuoted(t.mid(11).trimmed());
            if (parts.size() >= 4) {
                current.name     = unquote(parts[0]);
                current.type     = unquote(parts[1]);
                current.subtype  = unquote(parts[2]);
                current.category = unquote(parts[3]);
            }
            inItem = true;
            continue;
        }
        if (!inItem) continue;

        if (t.startsWith("ITEMMINLEVEL:"))
            current.minLevel = t.mid(13).trimmed().toInt();
    }
    saveItem();
}

void SchoolBuilderTab::parseSkills(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream ts(&f);

    SkillDef current;
    bool inSkill = false;

    auto saveSkill = [&]() {
        if (inSkill && !current.name.isEmpty() && !current.useClass.isEmpty())
            m_skills.append(current);
    };

    QString line;
    while (ts.readLineInto(&line)) {
        QString t = line.trimmed();
        if (t.isEmpty() || t.startsWith("//")) continue;

        if (t.startsWith("SKILLCREATE:")) {
            saveSkill();
            current = SkillDef{};
            // Extract first quoted string = skill name
            QString rest = t.mid(12).trimmed();
            int s = rest.indexOf('"');
            int e = rest.indexOf('"', s + 1);
            if (s >= 0 && e > s)
                current.name = rest.mid(s + 1, e - s - 1);
            inSkill = true;
            continue;
        }
        if (!inSkill) continue;

        if (t.startsWith("SKILLUSECLASS:"))
            current.useClass = unquote(t.mid(14));
        else if (t.startsWith("SKILLLEVEL:"))
            current.skillLevel = t.mid(11).trimmed().toInt();
        else if (t.startsWith("SKILLJOBPOINTCOST:"))
            current.jpCost = t.mid(18).trimmed().toInt();
    }
    saveSkill();
}

void SchoolBuilderTab::loadSchoolFile()
{
    m_gladiators.clear();
    m_schoolHeader.clear();

    QString path = resolvePath(m_moddedDir, "data", "school") + schoolFileName();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList lines = QTextStream(&f).readAll().split('\n');
    QStringList required = requiredNames();

    // Find first CREATEUNIT
    int firstUnit = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].trimmed().startsWith("CREATEUNIT:")) {
            firstUnit = i;
            break;
        }
    }

    if (firstUnit < 0) {
        m_schoolHeader = lines.join('\n');
        return;
    }

    for (int i = 0; i < firstUnit; ++i)
        m_schoolHeader += lines[i] + "\n";

    GladiatorEntry current;
    bool inUnit = false;

    for (int i = firstUnit; i < lines.size(); ++i) {
        QString t = lines[i].trimmed();

        if (t.startsWith("CREATEUNIT:")) {
            if (inUnit) m_gladiators.append(current);
            current = GladiatorEntry{};
            current.items = {"", "", "", "", ""};
            inUnit = true;

            QString rest = t.mid(11);
            int ci = rest.indexOf("//");
            if (ci >= 0) rest = rest.left(ci);
            QStringList parts = splitQuoted(rest);
            if (parts.size() >= 1) current.name      = unquote(parts[0]);
            if (parts.size() >= 2) current.className = unquote(parts[1]);
            current.isRequired = required.contains(current.name, Qt::CaseInsensitive);
            continue;
        }
        if (!inUnit) continue;

        if (t.startsWith("LEVEL:"))
            current.level = t.mid(6).trimmed().toInt();
        else if (t.startsWith("EXPERIENCE:"))
            current.experience = t.mid(11).trimmed().toInt();
        else if (t.startsWith("JOBPOINTS:"))
            current.jobPoints = t.mid(10).trimmed().toInt();
        else if (t.startsWith("CORESTATSCOMP2:"))
            current.coreStats = parseIntList(t.mid(15));
        else if (t.startsWith("ITEMSCOMP:")) {
            QString rest = t.mid(10).trimmed();
            int ci = rest.indexOf("//");
            if (ci >= 0) rest = rest.left(ci).trimmed();
            // Extract quoted strings
            QStringList names;
            QString cur;
            bool inQ = false;
            for (QChar c : rest) {
                if (c == '"') {
                    if (inQ) { names.append(cur); cur.clear(); }
                    inQ = !inQ;
                } else if (inQ) {
                    cur += c;
                }
            }
            while (names.size() < 5) names.append("");
            current.items = QVector<QString>(names.begin(), names.end());
        }
        else if (t.startsWith("SKILL:")) {
            QString sk = unquote(t.mid(6));
            if (!sk.isEmpty()) current.skills.append(sk);
        }
    }
    if (inUnit) m_gladiators.append(current);
}

// ── UI refresh ────────────────────────────────────────────────────────────────

void SchoolBuilderTab::refreshGladiatorList()
{
    int restoreRow = m_gladiatorList->currentRow();
    m_gladiatorList->clear();
    for (const GladiatorEntry &g : m_gladiators) {
        QString label = QString("%1  (%2)  —  Lv.%3")
                        .arg(g.name, g.className).arg(g.level);
        if (g.isRequired) label += "   [required]";
        auto *item = new QListWidgetItem(label, m_gladiatorList);
        if (g.isRequired)
            item->setForeground(QColor(120, 120, 120));
    }
    // Restore selection (clamped to valid range)
    if (restoreRow >= 0 && restoreRow < m_gladiatorList->count())
        m_gladiatorList->setCurrentRow(restoreRow);
}

void SchoolBuilderTab::refreshStats()
{
    QString className = m_classCombo->currentText();
    if (!m_classes.contains(className)) {
        m_statsEdit->setText("0, 0, 0, 0, 0, 0");
        m_jpSpin->setValue(5);
        return;
    }
    const ClassDef &cls = m_classes[className];
    int level = m_levelSpin->value();
    QVector<int> st = computeStats(cls, level);
    if (st.size() >= 6) {
        m_statsEdit->setText(
            QString("%1, %2, %3, %4, %5, %6")
            .arg(st[0]).arg(st[1]).arg(st[2]).arg(st[3]).arg(st[4]).arg(st[5]));
    }
    m_jpSpin->setValue(computeJP(cls, level));
}

void SchoolBuilderTab::refreshItemCombos()
{
    QString className = m_classCombo->currentText();
    int level = m_levelSpin->value();

    const QStringList slotNames  = {"weapon","armor","shield","helmet","accessory"};
    QComboBox * const combos[]   = {m_weaponCombo, m_armorCombo, m_shieldCombo,
                                    m_helmetCombo, m_accessoryCombo};

    if (!m_classes.contains(className)) {
        for (auto *c : combos) c->clear();
        return;
    }

    const ClassDef &cls = m_classes[className];
    for (int s = 0; s < slotNames.size(); ++s) {
        combos[s]->clear();
        combos[s]->addItem("(none)");
        QVector<ItemDef> its = itemsForSlot(cls, slotNames[s], level, isBypassMode());
        std::sort(its.begin(), its.end(),
                  [](const ItemDef &a, const ItemDef &b){ return a.name < b.name; });
        for (const ItemDef &it : its)
            combos[s]->addItem(it.name);
    }
}

void SchoolBuilderTab::refreshSkillList()
{
    QString className = m_classCombo->currentText();
    m_skillList->clear();

    if (!m_classes.contains(className)) return;

    const QString useClass = m_classes[className].skillUseName;

    for (const SkillDef &sk : m_skills) {
        if (sk.useClass.compare(useClass, Qt::CaseInsensitive) != 0) continue;

        auto *item = new QListWidgetItem(sk.name, m_skillList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);

        QString tip = sk.jpCost > 0
            ? QString("JP cost: %1").arg(sk.jpCost)
            : "JP cost: free";
        if (sk.skillLevel > 0)
            tip += QString("   (requires level %1)").arg(sk.skillLevel);
        item->setToolTip(tip);
    }
}

// ── Computation ───────────────────────────────────────────────────────────────

QVector<int> SchoolBuilderTab::computeStats(const ClassDef &cls, int level) const
{
    QVector<int> stats = cls.levelZeroStats;
    if (stats.isEmpty()) return stats;

    // Level 1 = LEVELZEROSTATS; each level up applies the corresponding row.
    // Row 0 = awards for reaching level 2, row 1 = level 3, etc.
    for (int lvl = 2; lvl <= level; ++lvl) {
        int rowIdx = lvl - 2;
        if (rowIdx >= cls.levelStatAwards.size()) break; // no more rows = no more gains
        const QVector<int> &row = cls.levelStatAwards[rowIdx];
        for (int j = 0; j < qMin(stats.size(), row.size()); ++j)
            stats[j] += row[j];
    }
    return stats;
}

int SchoolBuilderTab::computeJP(const ClassDef &cls, int level) const
{
    int jp = 5; // base at level 1
    for (int lvl = 2; lvl <= level; ++lvl) {
        int idx = lvl - 2;
        if (idx < cls.levelUpJpGiven.size())
            jp += cls.levelUpJpGiven[idx];
        else if (!cls.levelUpJpGiven.isEmpty())
            jp += cls.levelUpJpGiven.last(); // repeat last value for high levels
    }
    return jp;
}

bool SchoolBuilderTab::itemEquippable(int itemMinLevel, int gladiatorLevel) const
{
    if (gladiatorLevel >= 11) return true;
    return itemMinLevel <= gladiatorLevel;
}

bool SchoolBuilderTab::itemMatchesCat(const ItemDef &item, const ItemCatEntry &cat) const
{
    // Slot → type: "weapon" → "Weapon"
    QString itemType = cat.slot;
    if (!itemType.isEmpty()) itemType[0] = itemType[0].toUpper();
    if (item.type.compare(itemType, Qt::CaseInsensitive) != 0) return false;

    // Subtype: "<Any>" is a wildcard
    if (cat.subtype != "<Any>")
        if (item.subtype.compare(cat.subtype, Qt::CaseInsensitive) != 0) return false;

    // Category must match
    return item.category.compare(cat.category, Qt::CaseInsensitive) == 0;
}

bool SchoolBuilderTab::isBypassMode() const
{
    return m_bypassCheckbox && m_bypassCheckbox->isChecked();
}

QVector<ItemDef> SchoolBuilderTab::itemsForSlot(const ClassDef &cls,
                                                  const QString &slot,
                                                  int level,
                                                  bool bypassLevel) const
{
    QVector<ItemDef> result;
    QSet<QString> seen;

    for (const ItemDef &item : m_items) {
        if (!bypassLevel && !itemEquippable(item.minLevel, level)) continue;
        if (seen.contains(item.name)) continue;
        for (const ItemCatEntry &cat : cls.itemCats) {
            if (cat.slot != slot) continue;
            if (itemMatchesCat(item, cat)) {
                result.append(item);
                seen.insert(item.name);
                break;
            }
        }
    }
    return result;
}

// ── Edit helpers ──────────────────────────────────────────────────────────────

void SchoolBuilderTab::onGladiatorSelectionChanged(int row)
{
    if (row < 0 || row >= m_gladiators.size()) {
        m_editBtn->setEnabled(false);
        m_removeBtn->setEnabled(false);
        return;
    }
    bool required = m_gladiators[row].isRequired;
    m_editBtn->setEnabled(true);           // required gladiators can be edited
    m_removeBtn->setEnabled(!required);    // but not removed
}

void SchoolBuilderTab::editSelectedGladiator()
{
    int row = m_gladiatorList->currentRow();
    if (row < 0 || row >= m_gladiators.size()) return;
    loadGladiatorIntoForm(row);
}

void SchoolBuilderTab::removeSelectedGladiator()
{
    int row = m_gladiatorList->currentRow();
    if (row < 0 || row >= m_gladiators.size()) return;
    if (m_gladiators[row].isRequired) return;

    QString name = m_gladiators[row].name;
    auto reply = QMessageBox::question(this, "Remove gladiator",
        QString("Remove \"%1\" from the school?").arg(name),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    // If we were editing this entry, cancel edit mode first
    if (m_editingIndex == row) clearEditMode();

    m_gladiators.remove(row);
    refreshGladiatorList();

    // Adjust editing index if needed
    if (m_editingIndex > row) --m_editingIndex;
}

void SchoolBuilderTab::loadGladiatorIntoForm(int index)
{
    const GladiatorEntry &g = m_gladiators[index];
    m_editingIndex = index;

    m_nameEdit->setText(g.name);

    int classIdx = m_classCombo->findText(g.className, Qt::MatchFixedString);
    if (classIdx >= 0) {
        m_classCombo->blockSignals(true);
        m_classCombo->setCurrentIndex(classIdx);
        m_classCombo->blockSignals(false);
    }

    m_levelSpin->blockSignals(true);
    m_levelSpin->setValue(g.level);
    m_levelSpin->blockSignals(false);

    // Always recompute stats/JP for display, then overwrite with saved values
    refreshStats();
    const QVector<int> &st = g.coreStats;
    if (st.size() >= 6)
        m_statsEdit->setText(
            QString("%1, %2, %3, %4, %5, %6")
            .arg(st[0]).arg(st[1]).arg(st[2]).arg(st[3]).arg(st[4]).arg(st[5]));
    m_jpSpin->setValue(g.jobPoints);

    refreshItemCombos();
    refreshSkillList();

    // Restore item selections
    const QStringList slotNames = {"weapon","armor","shield","helmet","accessory"};
    QComboBox * const combos[]  = {m_weaponCombo, m_armorCombo, m_shieldCombo,
                                   m_helmetCombo, m_accessoryCombo};
    for (int s = 0; s < 5; ++s) {
        QString want = (s < g.items.size()) ? g.items[s] : "";
        int idx = want.isEmpty() ? 0 : combos[s]->findText(want, Qt::MatchFixedString);
        combos[s]->setCurrentIndex(qMax(0, idx));
    }

    // Restore skill checks
    QSet<QString> checked(g.skills.begin(), g.skills.end());
    for (int i = 0; i < m_skillList->count(); ++i) {
        auto *item = m_skillList->item(i);
        item->setCheckState(checked.contains(item->text()) ? Qt::Checked : Qt::Unchecked);
    }

    m_rightGroup->setTitle("Edit Gladiator");
    m_addBtn->setText("Update Gladiator");
    m_cancelEditBtn->show();

    // Highlight the row being edited
    m_gladiatorList->setCurrentRow(index);
}

void SchoolBuilderTab::clearEditMode()
{
    m_editingIndex = -1;
    m_rightGroup->setTitle("Add Gladiator");
    m_addBtn->setText("Add to School");
    m_cancelEditBtn->hide();
    m_nameEdit->clear();

    // Uncheck all skills
    for (int i = 0; i < m_skillList->count(); ++i)
        m_skillList->item(i)->setCheckState(Qt::Unchecked);

    m_gladiatorList->clearSelection();
    m_gladiatorList->setCurrentRow(-1);
}

// ── File helpers ──────────────────────────────────────────────────────────────

QString SchoolBuilderTab::resolvePath(const QString &base,
                                       const QString &sub1,
                                       const QString &sub2) const
{
    auto findDir = [](const QString &parent, const QString &name) -> QString {
        const auto entries = QDir(parent).entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &fi : entries)
            if (fi.fileName().compare(name, Qt::CaseInsensitive) == 0)
                return fi.absoluteFilePath() + "/";
        return parent + (parent.endsWith('/') ? "" : "/") + name + "/";
    };

    QString path = findDir(base, sub1);
    if (!sub2.isEmpty()) path = findDir(path, sub2);
    return path;
}

QString SchoolBuilderTab::schoolFileName() const
{
    return m_valensRadio->isChecked() ? "valensimperia.tok" : "ursulanordagh.tok";
}

QStringList SchoolBuilderTab::requiredNames() const
{
    return m_valensRadio->isChecked()
        ? QStringList{"Valens", "Ludo"}
        : QStringList{"Ursula", "Urlan"};
}

QString SchoolBuilderTab::formatGladiator(const GladiatorEntry &g) const
{
    QString out;
    QTextStream ts(&out);

    QVector<QString> items = g.items;
    while (items.size() < 5) items.append("");

    ts << "CREATEUNIT:\t\t\"" << g.name << "\", \"" << g.className
       << "\"\t\t// Name, class\n";
    ts << "\tLEVEL:\t\t"     << g.level      << "\n";
    ts << "\tEXPERIENCE:\t"  << g.experience << "\n";
    ts << "\tJOBPOINTS:\t"   << g.jobPoints  << "\n";
    ts << "\tCUSTOMIZE:\t1, \"\"\n";
    ts << "\n";
    ts << "\t//            \tCON, PWR, ACC, DEF, INT, MOVE\n";
    if (g.coreStats.size() >= 6) {
        ts << "\tCORESTATSCOMP2:\t"
           << g.coreStats[0] << ", " << g.coreStats[1] << ", "
           << g.coreStats[2] << ", " << g.coreStats[3] << ", "
           << g.coreStats[4] << ", " << g.coreStats[5] << "\n";
    }
    ts << "\n";
    ts << "\t\t\t\t//\tweapon,\tarmor,\tshield,\themet,\taccessory\n";
    ts << "\tITEMSCOMP:\t\t\t\"" << items[0] << "\", \""
       << items[1] << "\", \"" << items[2] << "\", \""
       << items[3] << "\", \"" << items[4] << "\"\n";

    if (!g.skills.isEmpty()) {
        ts << "\n\t// skills\n";
        for (const QString &sk : g.skills)
            ts << "\tSKILL: \"" << sk << "\"\n";
    }
    ts << "\n";
    return out;
}
