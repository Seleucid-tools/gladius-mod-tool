#pragma once
#include <QWidget>
#include <QString>
#include <QVector>
#include <QMap>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QSpinBox;
class QListWidget;
class QLabel;
class QPushButton;
class QRadioButton;

// ── Data structures ────────────────────────────────────────────────────────────

struct ItemCatEntry {
    QString slot;      // weapon | armor | shield | helmet | accessory
    QString subtype;   // Sword | <Any> | Bow | etc.
    QString category;  // Medium | Plain | Ludo | etc.
};

struct ClassDef {
    QString               name;
    QString               skillUseName;
    QVector<int>          levelZeroStats;    // CON PWR ACC DEF INT MOVE at level 1
    QVector<QVector<int>> levelStatAwards;   // stat gains per level-up (row 0 = lv2, etc.)
    QVector<int>          levelUpJpGiven;    // JP awarded per level-up
    QVector<ItemCatEntry> itemCats;
};

struct ItemDef {
    QString name;
    QString type;      // Weapon | Armor | Helmet | Shield | Accessory
    QString subtype;   // Sword | Bow | Armor | etc.
    QString category;  // Medium | Plain | etc.
    int     minLevel = 1;
};

struct SkillDef {
    QString name;
    QString useClass;
    int     skillLevel = 0;
    int     jpCost     = 0;
};

struct GladiatorEntry {
    QString          name;
    QString          className;
    int              level      = 1;
    int              experience = 0;
    int              jobPoints  = 5;
    QVector<int>     coreStats;               // CON PWR ACC DEF INT MOVE
    QVector<QString> items;                   // weapon armor shield helmet accessory
    QVector<QString> skills;
    bool             isRequired = false;
};

// ── Tab widget ─────────────────────────────────────────────────────────────────

class SchoolBuilderTab : public QWidget
{
    Q_OBJECT
public:
    explicit SchoolBuilderTab(QWidget *parent = nullptr);

public slots:
    void setRootPath(const QString &moddedDir, const QString &vanillaDir);

private slots:
    void onSchoolChanged();
    void onClassChanged(int index);
    void onLevelChanged(int value);
    void addGladiator();
    void onSaveSchool();
    void onGladiatorSelectionChanged(int row);
    void editSelectedGladiator();
    void removeSelectedGladiator();
    void onBypassToggled(bool checked);

private:
    // ── State ─────────────────────────────────────────────────────────────────
    QString                  m_moddedDir;
    QMap<QString, ClassDef>  m_classes;
    QVector<ItemDef>         m_items;
    QVector<SkillDef>        m_skills;
    QString                  m_schoolHeader;
    QVector<GladiatorEntry>  m_gladiators;
    int                      m_editingIndex = -1;

    // ── UI ────────────────────────────────────────────────────────────────────
    QRadioButton *m_valensRadio;
    QRadioButton *m_ursulaRadio;
    QPushButton  *m_saveBtn;
    QCheckBox    *m_bypassCheckbox;
    QLabel       *m_hintLabel;
    QWidget      *m_mainWidget;

    // Left panel
    QListWidget  *m_gladiatorList;
    QPushButton  *m_editBtn;
    QPushButton  *m_removeBtn;

    // Right form
    QLineEdit    *m_nameEdit;
    QComboBox    *m_classCombo;
    QSpinBox     *m_levelSpin;
    QLineEdit    *m_statsEdit;   // read-only normally; editable in bypass mode
    QSpinBox     *m_jpSpin;      // disabled normally; enabled in bypass mode
    QComboBox    *m_weaponCombo;
    QComboBox    *m_armorCombo;
    QComboBox    *m_shieldCombo;
    QComboBox    *m_helmetCombo;
    QComboBox    *m_accessoryCombo;
    QListWidget  *m_skillList;
    QPushButton  *m_addBtn;
    QPushButton  *m_cancelEditBtn;
    QGroupBox    *m_rightGroup;

    // ── Parsers ───────────────────────────────────────────────────────────────
    void parseClassDefs(const QString &path);
    void parseItems    (const QString &path);
    void parseSkills   (const QString &path);
    void loadSchoolFile();

    // ── UI refresh ────────────────────────────────────────────────────────────
    void refreshGladiatorList();
    void refreshItemCombos();
    void refreshSkillList();
    void refreshStats();

    // ── Computation ───────────────────────────────────────────────────────────
    QVector<int>     computeStats  (const ClassDef &cls, int level) const;
    int              computeJP     (const ClassDef &cls, int level) const;
    QVector<ItemDef> itemsForSlot  (const ClassDef &cls,
                                    const QString &slot, int level,
                                    bool bypassLevel = false) const;
    bool itemMatchesCat (const ItemDef &item, const ItemCatEntry &cat) const;
    bool itemEquippable (int itemMinLevel, int gladiatorLevel) const;
    bool isBypassMode   () const;

    // ── Edit helpers ──────────────────────────────────────────────────────────
    void loadGladiatorIntoForm(int index);
    void clearEditMode();

    // ── File helpers ──────────────────────────────────────────────────────────
    QString     resolvePath    (const QString &base, const QString &sub1,
                                const QString &sub2 = {}) const;
    QString     schoolFileName () const;
    QStringList requiredNames  () const;
    QString     formatGladiator(const GladiatorEntry &g) const;
};
