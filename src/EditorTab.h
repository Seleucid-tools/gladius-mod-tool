#pragma once
#include <QWidget>
#include <QString>
#include <QVector>

class QFileSystemModel;
class QTreeView;
class QPlainTextEdit;
class QPushButton;
class QLabel;
class QTabWidget;
class QSortFilterProxyModel;
class QLineEdit;

// Per-tab state: each open file gets one of these
struct FileTab {
    QString        path;       // absolute path in moddedDir
    QPlainTextEdit *editor;    // owned by the QTabWidget page widget
    bool           dirty = false;
};

class EditorTab : public QWidget
{
    Q_OBJECT
public:
    explicit EditorTab(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

public slots:
    void setRootPath(const QString &moddedDir, const QString &vanillaDir);
    void showFindBar();

private slots:
    void onFileSelected(const QModelIndex &index);
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void saveCurrentFile();
    void saveAllFiles();
    void reloadCurrentFile();
    void revertCurrentFile();
    void hideFindBar();
    void findNext();
    void findPrev();
    void onFindTextChanged(const QString &text);

private:
    // ── Tree widgets ─────────────────────────────────────────────────────────
    QFileSystemModel      *m_model;
    QSortFilterProxyModel *m_proxy;
    QTreeView             *m_tree;

    // ── Multi-file tab bar ────────────────────────────────────────────────────
    QTabWidget *m_fileTabs;   // each page is a QPlainTextEdit

    // ── Toolbar buttons ───────────────────────────────────────────────────────
    QPushButton *m_saveBtn;
    QPushButton *m_saveAllBtn;
    QPushButton *m_reloadBtn;
    QPushButton *m_revertBtn;

    // ── Hint label (shown before any ISO is unpacked) ─────────────────────────
    QLabel *m_hintLabel;

    // ── State ─────────────────────────────────────────────────────────────────
    QString m_moddedDir;
    QString m_vanillaDir;

    QVector<FileTab> m_tabs;  // parallel to m_fileTabs pages

    // ── Helpers ───────────────────────────────────────────────────────────────
    int     currentTabIndex() const;
    FileTab *currentTab();

    // Open file in new tab (or switch to existing tab if already open)
    void openFile(const QString &path);

    // Load from disk into the given tab's editor (resets dirty state)
    void loadIntoTab(int index);

    // Update tab label to reflect filename + dirty marker
    void updateTabLabel(int index);

    // Dirty state for a specific tab
    void setTabDirty(int index, bool dirty);

    // Returns true if all unsaved changes were handled (saved or discarded)
    bool confirmClose(int index);

    QString vanillaPath(const QString &moddedFilePath) const;
    void updateToolbar();

    // ── Find bar ──────────────────────────────────────────────────────────────
    QWidget   *m_findBar;
    QLineEdit *m_findInput;
    QLabel    *m_findMatchLabel;

    QPlainTextEdit *currentEditor() const;
    void highlightMatches(QPlainTextEdit *ed, const QString &text);
};
