#include "EditorTab.h"

#include <QFileSystemModel>
#include <QTreeView>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QTabBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QLineEdit>
#include <QKeyEvent>
#include <QEvent>
#include <QShortcut>
#include <QTextDocument>
#include <QTextCursor>
#include <QFileDialog>
#include <QFrame>
#include <QFormLayout>

// ── File type filter ──────────────────────────────────────────────────────────
class TextFileFilter : public QSortFilterProxyModel
{
public:
    explicit TextFileFilter(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent) {}

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        QModelIndex idx = sourceModel()->index(row, 0, parent);
        auto *m = qobject_cast<QFileSystemModel *>(sourceModel());
        if (!m) return true;
        if (m->isDir(idx)) return true;

        static const QStringList exts = {
            "txt", "tok", "bin", "brf", "cfg", "ini", "xml", "json"
        };
        QString ext = idx.data().toString().section('.', -1).toLower();
        return exts.contains(ext);
    }
};

// ── EditorTab ─────────────────────────────────────────────────────────────────

EditorTab::EditorTab(QWidget *parent)
    : QWidget(parent)
{
    // ── File system model ─────────────────────────────────────────────────────
    m_model = new QFileSystemModel(this);
    m_model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);

    m_proxy = new TextFileFilter(this);
    m_proxy->setSourceModel(m_model);

    // ── Tree ──────────────────────────────────────────────────────────────────
    m_tree = new QTreeView(this);
    m_tree->setModel(m_proxy);
    m_tree->setColumnHidden(1, true);
    m_tree->setColumnHidden(2, true);
    m_tree->setColumnHidden(3, true);
    m_tree->header()->hide();
    m_tree->setMinimumWidth(180);
    m_tree->setMaximumWidth(320);
    m_tree->hide();

    connect(m_tree, &QTreeView::clicked, this, &EditorTab::onFileSelected);

    // ── File tab widget ───────────────────────────────────────────────────────
    m_fileTabs = new QTabWidget(this);
    m_fileTabs->setTabsClosable(true);
    m_fileTabs->setMovable(true);
    m_fileTabs->setDocumentMode(true);  // cleaner look on Linux

    connect(m_fileTabs, &QTabWidget::currentChanged,   this, &EditorTab::onTabChanged);
    connect(m_fileTabs, &QTabWidget::tabCloseRequested, this, &EditorTab::onTabCloseRequested);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    m_saveBtn    = new QPushButton("Save",           this);
    m_saveAllBtn = new QPushButton("Save All",       this);
    m_reloadBtn  = new QPushButton("Reload",         this);
    m_revertBtn  = new QPushButton("Revert to vanilla", this);

    m_saveBtn->setEnabled(false);
    m_saveAllBtn->setEnabled(false);
    m_reloadBtn->setEnabled(false);
    m_revertBtn->setEnabled(false);

    m_saveBtn->setToolTip("Save the current file (Ctrl+S)");
    m_saveAllBtn->setToolTip("Save all modified files");
    m_reloadBtn->setToolTip("Reload current file from disk, discarding unsaved changes");
    m_revertBtn->setToolTip("Overwrite current file with the vanilla (original) version");

    m_saveBtn->setShortcut(QKeySequence("Ctrl+S"));
    m_saveAllBtn->setShortcut(QKeySequence("Ctrl+Shift+S"));

    connect(m_saveBtn,    &QPushButton::clicked, this, &EditorTab::saveCurrentFile);
    connect(m_saveAllBtn, &QPushButton::clicked, this, &EditorTab::saveAllFiles);
    connect(m_reloadBtn,  &QPushButton::clicked, this, &EditorTab::reloadCurrentFile);
    connect(m_revertBtn,  &QPushButton::clicked, this, &EditorTab::revertCurrentFile);

    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(4, 2, 4, 2);
    toolbar->addStretch(1);
    toolbar->addWidget(m_reloadBtn);
    toolbar->addWidget(m_revertBtn);
    toolbar->addWidget(m_saveAllBtn);
    toolbar->addWidget(m_saveBtn);

    // ── Find bar ──────────────────────────────────────────────────────────────
    m_findBar = new QWidget(this);
    m_findInput = new QLineEdit(m_findBar);
    m_findInput->setPlaceholderText("Find…");
    m_findInput->setFixedWidth(220);

    auto *findPrevBtn = new QPushButton("▲", m_findBar);
    auto *findNextBtn = new QPushButton("▼", m_findBar);
    findPrevBtn->setFixedWidth(28);
    findNextBtn->setFixedWidth(28);
    findPrevBtn->setToolTip("Previous match (Shift+F3)");
    findNextBtn->setToolTip("Next match (F3 / Enter)");

    m_findMatchLabel = new QLabel(m_findBar);
    m_findMatchLabel->setMinimumWidth(80);

    auto *findCloseBtn = new QPushButton("✕", m_findBar);
    findCloseBtn->setFixedWidth(24);
    findCloseBtn->setToolTip("Close (Escape)");
    findCloseBtn->setFlat(true);

    auto *findLayout = new QHBoxLayout(m_findBar);
    findLayout->setContentsMargins(6, 3, 6, 3);
    findLayout->addWidget(new QLabel("Find:"));
    findLayout->addWidget(m_findInput);
    findLayout->addWidget(findPrevBtn);
    findLayout->addWidget(findNextBtn);
    findLayout->addWidget(m_findMatchLabel);
    findLayout->addStretch(1);
    findLayout->addWidget(findCloseBtn);
    m_findBar->hide();

    connect(m_findInput, &QLineEdit::textChanged,  this, &EditorTab::onFindTextChanged);
    connect(m_findInput, &QLineEdit::returnPressed, this, &EditorTab::findNext);
    connect(findNextBtn, &QPushButton::clicked,     this, &EditorTab::findNext);
    connect(findPrevBtn, &QPushButton::clicked,     this, &EditorTab::findPrev);
    connect(findCloseBtn,&QPushButton::clicked,     this, &EditorTab::hideFindBar);

    // Ctrl+F shortcut (works even when the editor has focus)
    auto *findShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    findShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(findShortcut, &QShortcut::activated, this, &EditorTab::showFindBar);

    auto *f3 = new QShortcut(QKeySequence("F3"), this);
    f3->setContext(Qt::WidgetWithChildrenShortcut);
    connect(f3, &QShortcut::activated, this, &EditorTab::findNext);

    auto *shiftF3 = new QShortcut(QKeySequence("Shift+F3"), this);
    shiftF3->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shiftF3, &QShortcut::activated, this, &EditorTab::findPrev);

    // Install event filter on the find input to handle Escape
    m_findInput->installEventFilter(this);

    // ── Working directory bar ─────────────────────────────────────────────────
    m_moddedDirEdit = new QLineEdit(this);
    m_moddedDirEdit->setReadOnly(true);
    m_moddedDirEdit->setPlaceholderText("No working directory set — unpack an ISO or browse below");

    m_vanillaDirEdit = new QLineEdit(this);
    m_vanillaDirEdit->setReadOnly(true);
    m_vanillaDirEdit->setPlaceholderText("Optional — required for Revert to vanilla");

    auto *moddedBrowseBtn  = new QPushButton("Browse…", this);
    auto *vanillaBrowseBtn = new QPushButton("Browse…", this);
    moddedBrowseBtn->setFixedWidth(80);
    vanillaBrowseBtn->setFixedWidth(80);
    moddedBrowseBtn->setToolTip("Select the modded files directory");
    vanillaBrowseBtn->setToolTip("Select the vanilla (original) files directory — enables Revert");

    connect(moddedBrowseBtn,  &QPushButton::clicked, this, &EditorTab::browseModdedDir);
    connect(vanillaBrowseBtn, &QPushButton::clicked, this, &EditorTab::browseVanillaDir);

    auto *moddedRow  = new QHBoxLayout;
    moddedRow->addWidget(new QLabel("Modded dir:"),  0);
    moddedRow->addWidget(m_moddedDirEdit,            1);
    moddedRow->addWidget(moddedBrowseBtn,            0);

    auto *vanillaRow = new QHBoxLayout;
    vanillaRow->addWidget(new QLabel("Vanilla dir:"), 0);
    vanillaRow->addWidget(m_vanillaDirEdit,           1);
    vanillaRow->addWidget(vanillaBrowseBtn,           0);

    auto *dirBarLayout = new QVBoxLayout;
    dirBarLayout->setContentsMargins(6, 4, 6, 4);
    dirBarLayout->setSpacing(3);
    dirBarLayout->addLayout(moddedRow);
    dirBarLayout->addLayout(vanillaRow);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);

    // ── Hint label ────────────────────────────────────────────────────────────
    m_hintLabel = new QLabel(
        "Unpack a vanilla ISO first, or use Browse above to open an existing extraction.\n"
        "Only .txt, .tok and similar text files are shown.", this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setWordWrap(true);

    // ── Editor pane: toolbar + tabs (or hint) ─────────────────────────────────
    auto *editorPane   = new QWidget(this);
    auto *editorLayout = new QVBoxLayout(editorPane);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    editorLayout->addLayout(toolbar);
    editorLayout->addWidget(m_hintLabel, 1);
    editorLayout->addWidget(m_fileTabs, 1);
    editorLayout->addWidget(m_findBar);
    m_fileTabs->hide();

    // ── Horizontal splitter: tree left, editor right ──────────────────────────
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_tree);
    splitter->addWidget(editorPane);
    splitter->setStretchFactor(1, 3);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(dirBarLayout);
    layout->addWidget(separator);
    layout->addWidget(splitter, 1);
}

// ── Public API ────────────────────────────────────────────────────────────────

void EditorTab::setRootPath(const QString &moddedDir, const QString &vanillaDir)
{
    // Close open file tabs when switching to a different modded directory
    if (moddedDir != m_moddedDir && !m_tabs.isEmpty()) {
        for (int i = m_tabs.size() - 1; i >= 0; --i) {
            if (!confirmClose(i)) return;  // user cancelled
            m_fileTabs->removeTab(i);
            m_tabs.removeAt(i);
        }
    }

    m_moddedDir  = moddedDir;
    m_vanillaDir = vanillaDir;

    m_moddedDirEdit->setText(moddedDir);
    m_vanillaDirEdit->setText(vanillaDir);

    QModelIndex root = m_model->setRootPath(moddedDir);
    m_tree->setRootIndex(m_proxy->mapFromSource(root));

    m_tree->show();
    m_hintLabel->hide();
    m_fileTabs->show();

    emit workingDirChanged(moddedDir, vanillaDir);
}

// ── Browse slots ──────────────────────────────────────────────────────────────

void EditorTab::browseModdedDir()
{
    QString start = m_moddedDir.isEmpty() ? QDir::homePath() : m_moddedDir;
    QString dir = QFileDialog::getExistingDirectory(
        this, "Select Modded Directory", start);
    if (!dir.isEmpty())
        setRootPath(dir, m_vanillaDir);
}

void EditorTab::browseVanillaDir()
{
    QString start = m_vanillaDir.isEmpty()
        ? (m_moddedDir.isEmpty() ? QDir::homePath() : m_moddedDir)
        : m_vanillaDir;
    QString dir = QFileDialog::getExistingDirectory(
        this, "Select Vanilla Directory", start);
    if (!dir.isEmpty()) {
        m_vanillaDir = dir;
        m_vanillaDirEdit->setText(dir);
        updateToolbar();
        emit workingDirChanged(m_moddedDir, m_vanillaDir);
    }
}

// ── Private slots ─────────────────────────────────────────────────────────────

void EditorTab::onFileSelected(const QModelIndex &proxyIndex)
{
    QModelIndex idx = m_proxy->mapToSource(proxyIndex);
    if (m_model->isDir(idx)) return;
    openFile(m_model->filePath(idx));
}

void EditorTab::onTabChanged(int /*index*/)
{
    updateToolbar();
    if (m_findBar->isVisible())
        highlightMatches(currentEditor(), m_findInput->text());
}

void EditorTab::onTabCloseRequested(int index)
{
    if (!confirmClose(index)) return;

    m_fileTabs->removeTab(index);
    m_tabs.removeAt(index);

    updateToolbar();
}

void EditorTab::saveCurrentFile()
{
    int i = currentTabIndex();
    if (i < 0) return;

    FileTab &ft = m_tabs[i];
    QFile f(ft.path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Save failed", "Cannot write:\n" + ft.path);
        return;
    }
    QTextStream ts(&f);
    ts << ft.editor->toPlainText();
    setTabDirty(i, false);
}

void EditorTab::saveAllFiles()
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (!m_tabs[i].dirty) continue;
        QFile f(m_tabs[i].path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Save failed",
                "Cannot write:\n" + m_tabs[i].path);
            continue;
        }
        QTextStream ts(&f);
        ts << m_tabs[i].editor->toPlainText();
        setTabDirty(i, false);
    }
}

void EditorTab::reloadCurrentFile()
{
    int i = currentTabIndex();
    if (i < 0) return;

    if (m_tabs[i].dirty) {
        auto btn = QMessageBox::question(this, "Unsaved changes",
            "Reload from disk and discard unsaved changes?",
            QMessageBox::Yes | QMessageBox::No);
        if (btn != QMessageBox::Yes) return;
    }
    loadIntoTab(i);
}

void EditorTab::revertCurrentFile()
{
    int i = currentTabIndex();
    if (i < 0) return;

    const FileTab &ft = m_tabs[i];
    QString vanilla = vanillaPath(ft.path);

    if (vanilla.isEmpty() || !QFile::exists(vanilla)) {
        QMessageBox::warning(this, "Revert",
            "No vanilla version found.\n\nExpected:\n" + vanilla);
        return;
    }

    QString rel = QDir(m_moddedDir).relativeFilePath(ft.path);
    auto btn = QMessageBox::question(this, "Revert to vanilla",
        QString("Overwrite:\n  %1\n\nwith the vanilla version? "
                "Unsaved changes will be lost.").arg(rel),
        QMessageBox::Yes | QMessageBox::No);
    if (btn != QMessageBox::Yes) return;

    QFile::remove(ft.path);
    if (!QFile::copy(vanilla, ft.path)) {
        QMessageBox::critical(this, "Revert failed",
            "Could not copy vanilla file to:\n" + ft.path);
        return;
    }
    loadIntoTab(i);
}

// ── Private helpers ───────────────────────────────────────────────────────────

int EditorTab::currentTabIndex() const
{
    return m_fileTabs->currentIndex();  // -1 if no tabs
}

FileTab *EditorTab::currentTab()
{
    int i = currentTabIndex();
    return (i >= 0 && i < m_tabs.size()) ? &m_tabs[i] : nullptr;
}

void EditorTab::openFile(const QString &path)
{
    // If already open, just switch to that tab
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].path == path) {
            m_fileTabs->setCurrentIndex(i);
            return;
        }
    }

    // Create editor widget for new tab
    auto *editor = new QPlainTextEdit;
    QFont mono("Monospace");
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(10);
    editor->setFont(mono);

    // Build tab label from filename only (tooltip shows full relative path)
    QString name = QFileInfo(path).fileName();
    QString rel  = m_moddedDir.isEmpty() ? path
                 : QDir(m_moddedDir).relativeFilePath(path);

    int tabIndex = m_fileTabs->addTab(editor, name);
    m_fileTabs->setTabToolTip(tabIndex, rel);
    m_fileTabs->setCurrentIndex(tabIndex);

    FileTab ft;
    ft.path   = path;
    ft.editor = editor;
    ft.dirty  = false;
    m_tabs.append(ft);

    // Wire dirty tracking — use the tab index captured at open time
    // We need the index at signal time, so look it up by pointer.
    connect(editor, &QPlainTextEdit::textChanged, this, [this, editor]() {
        for (int i = 0; i < m_tabs.size(); ++i) {
            if (m_tabs[i].editor == editor && !m_tabs[i].dirty) {
                setTabDirty(i, true);
                break;
            }
        }
    });

    loadIntoTab(tabIndex);
}

void EditorTab::loadIntoTab(int index)
{
    if (index < 0 || index >= m_tabs.size()) return;
    FileTab &ft = m_tabs[index];

    QFile f(ft.path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open:\n" + ft.path);
        return;
    }
    QTextStream ts(&f);
    QString content = ts.readAll();

    ft.editor->blockSignals(true);
    ft.editor->setPlainText(content);
    ft.editor->blockSignals(false);

    setTabDirty(index, false);
    updateToolbar();
}

void EditorTab::updateTabLabel(int index)
{
    if (index < 0 || index >= m_tabs.size()) return;
    const FileTab &ft = m_tabs[index];
    QString name = QFileInfo(ft.path).fileName();
    m_fileTabs->setTabText(index, ft.dirty ? name + " ●" : name);
}

void EditorTab::setTabDirty(int index, bool dirty)
{
    if (index < 0 || index >= m_tabs.size()) return;
    m_tabs[index].dirty = dirty;
    updateTabLabel(index);
    updateToolbar();
}

bool EditorTab::confirmClose(int index)
{
    if (index < 0 || index >= m_tabs.size()) return true;
    if (!m_tabs[index].dirty) return true;

    QString name = QFileInfo(m_tabs[index].path).fileName();
    auto choice = QMessageBox::question(this, "Unsaved changes",
        QString("'%1' has unsaved changes.\nSave before closing?").arg(name),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (choice == QMessageBox::Save) {
        int prev = m_fileTabs->currentIndex();
        m_fileTabs->setCurrentIndex(index);
        saveCurrentFile();
        m_fileTabs->setCurrentIndex(prev);
        return true;
    }
    return (choice == QMessageBox::Discard);
}

void EditorTab::updateToolbar()
{
    int i = currentTabIndex();
    bool hasTab   = (i >= 0 && i < m_tabs.size());
    bool isDirty  = hasTab && m_tabs[i].dirty;
    bool anyDirty = std::any_of(m_tabs.begin(), m_tabs.end(),
                                [](const FileTab &ft){ return ft.dirty; });

    m_saveBtn->setEnabled(isDirty);
    m_saveAllBtn->setEnabled(anyDirty);
    m_reloadBtn->setEnabled(hasTab);

    if (hasTab) {
        QString vp = vanillaPath(m_tabs[i].path);
        m_revertBtn->setEnabled(!vp.isEmpty() && QFile::exists(vp));
    } else {
        m_revertBtn->setEnabled(false);
    }
}

QString EditorTab::vanillaPath(const QString &moddedFilePath) const
{
    if (moddedFilePath.isEmpty() || m_moddedDir.isEmpty() || m_vanillaDir.isEmpty())
        return {};
    QString rel = QDir(m_moddedDir).relativeFilePath(moddedFilePath);
    if (rel.startsWith("..")) return {};
    return QDir(m_vanillaDir).absoluteFilePath(rel);
}

// ── Find bar ──────────────────────────────────────────────────────────────────

QPlainTextEdit *EditorTab::currentEditor() const
{
    int i = m_fileTabs->currentIndex();
    if (i < 0 || i >= m_tabs.size()) return nullptr;
    return m_tabs[i].editor;
}

void EditorTab::showFindBar()
{
    m_findBar->show();
    m_findInput->setFocus();
    m_findInput->selectAll();
    highlightMatches(currentEditor(), m_findInput->text());
}

void EditorTab::hideFindBar()
{
    m_findBar->hide();
    // Clear highlights from all editors
    for (auto &ft : m_tabs)
        ft.editor->setExtraSelections({});
    m_findMatchLabel->clear();
    if (auto *ed = currentEditor())
        ed->setFocus();
}

void EditorTab::onFindTextChanged(const QString &text)
{
    highlightMatches(currentEditor(), text);
}

void EditorTab::highlightMatches(QPlainTextEdit *ed, const QString &text)
{
    if (!ed) {
        m_findMatchLabel->clear();
        return;
    }

    QList<QTextEdit::ExtraSelection> selections;

    if (!text.isEmpty()) {
        QTextDocument *doc = ed->document();
        QTextCursor cursor(doc);

        QTextCharFormat allMatchFmt;
        allMatchFmt.setBackground(QColor(255, 220, 0));   // yellow
        allMatchFmt.setForeground(QColor(0, 0, 0));

        int count = 0;
        while (true) {
            cursor = doc->find(text, cursor, QTextDocument::FindFlags{});
            if (cursor.isNull()) break;
            ++count;
            QTextEdit::ExtraSelection sel;
            sel.format = allMatchFmt;
            sel.cursor = cursor;
            selections.append(sel);
        }

        m_findMatchLabel->setText(count == 0
            ? QString("<font color='red'>No results</font>")
            : QString("%1 match%2").arg(count).arg(count == 1 ? "" : "es"));
    } else {
        m_findMatchLabel->clear();
    }

    ed->setExtraSelections(selections);
}

void EditorTab::findNext()
{
    QPlainTextEdit *ed = currentEditor();
    if (!ed) return;
    const QString text = m_findInput->text();
    if (text.isEmpty()) return;

    QTextCursor cursor = ed->document()->find(text, ed->textCursor());
    if (cursor.isNull()) {
        // Wrap around from start
        QTextCursor start(ed->document());
        cursor = ed->document()->find(text, start);
    }
    if (!cursor.isNull())
        ed->setTextCursor(cursor);
}

void EditorTab::findPrev()
{
    QPlainTextEdit *ed = currentEditor();
    if (!ed) return;
    const QString text = m_findInput->text();
    if (text.isEmpty()) return;

    QTextCursor cursor = ed->document()->find(
        text, ed->textCursor(), QTextDocument::FindBackward);
    if (cursor.isNull()) {
        // Wrap around from end
        QTextCursor end(ed->document());
        end.movePosition(QTextCursor::End);
        cursor = ed->document()->find(text, end, QTextDocument::FindBackward);
    }
    if (!cursor.isNull())
        ed->setTextCursor(cursor);
}

bool EditorTab::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_findInput && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape) {
            hideFindBar();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
