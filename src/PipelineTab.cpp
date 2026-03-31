#include "PipelineTab.h"
#include "NativeRunner.h"
#include "XisoRunner.h"
#include "Ps2IsoRunner.h"
#include "LogPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QThread>
#include <QMessageBox>
#include <QDate>
#include <QSettings>

PipelineTab::PipelineTab(Platform platform, LogPanel *log, QWidget *parent)
    : QWidget(parent), m_platform(platform), m_log(log)
{
    // ── ISO path row ─────────────────────────────────────────────────────────
    auto *isoGroup = new QGroupBox("Vanilla ISO", this);
    m_isoPathEdit = new QLineEdit(isoGroup);
    m_isoPathEdit->setPlaceholderText("Select your vanilla ISO file...");
    m_browseBtn = new QPushButton("Browse…", isoGroup);
    m_browseBtn->setFixedWidth(80);
    connect(m_browseBtn, &QPushButton::clicked, this, &PipelineTab::browseIso);

    auto *isoRow = new QHBoxLayout;
    isoRow->addWidget(m_isoPathEdit);
    isoRow->addWidget(m_browseBtn);
    isoGroup->setLayout(isoRow);

    // ── Modded directory row ──────────────────────────────────────────────────
    auto *moddedGroup = new QGroupBox("Modded BEC directory (for pack)", this);
    moddedGroup->setToolTip(
        "Directory containing your modded files (the \"working_BEC\" folder).\n"
        "Set this to repack a specific extraction when managing multiple sessions.\n"
        "If left empty the tool auto-detects the most recent session for the selected ISO.");
    m_moddedDirEdit = new QLineEdit(moddedGroup);
    m_moddedDirEdit->setPlaceholderText("Auto-detect from ISO path (leave empty to use most recent session)");
    m_moddedDirBrowseBtn = new QPushButton("Browse…", moddedGroup);
    m_moddedDirBrowseBtn->setFixedWidth(80);
    connect(m_moddedDirBrowseBtn, &QPushButton::clicked, this, &PipelineTab::browseModdedDir);

    auto *moddedRow = new QHBoxLayout;
    moddedRow->addWidget(m_moddedDirEdit);
    moddedRow->addWidget(m_moddedDirBrowseBtn);
    moddedGroup->setLayout(moddedRow);

    // ── Action buttons ────────────────────────────────────────────────────────
    auto *actGroup = new QGroupBox("Pipeline", this);
    m_unpackBtn = new QPushButton("Unpack vanilla ISO", actGroup);
    m_packBtn   = new QPushButton("Pack modded ISO",    actGroup);
    m_statusLabel = new QLabel("Ready.", actGroup);
    m_statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    connect(m_unpackBtn, &QPushButton::clicked, this, &PipelineTab::unpackVanilla);
    connect(m_packBtn,   &QPushButton::clicked, this, &PipelineTab::packModded);

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_unpackBtn);
    btnRow->addWidget(m_packBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_statusLabel);
    actGroup->setLayout(btnRow);

    // ── Main layout ───────────────────────────────────────────────────────────
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(isoGroup);
    layout->addWidget(moddedGroup);
    layout->addWidget(actGroup);
    layout->addStretch();

    // ── Restore persisted ISO path ────────────────────────────────────────────
    QSettings s("GladiusModTool", "MainWindow");
    QString key = QString("isoPath/%1").arg(platformStr());
    QString saved = s.value(key).toString();
    if (!saved.isEmpty() && QFile::exists(saved))
        m_isoPathEdit->setText(saved);

    connect(m_isoPathEdit, &QLineEdit::textChanged, this, [this, key](const QString &text) {
        QSettings s2("GladiusModTool", "MainWindow");
        s2.setValue(key, text);
    });

    // ── Restore persisted modded dir ──────────────────────────────────────────
    QString moddedKey = QString("moddedDir/%1").arg(platformStr());
    QString savedModded = s.value(moddedKey).toString();
    if (!savedModded.isEmpty() && QDir(savedModded).exists())
        m_moddedDirEdit->setText(savedModded);

    connect(m_moddedDirEdit, &QLineEdit::textChanged, this, [this, moddedKey](const QString &text) {
        QSettings s2("GladiusModTool", "MainWindow");
        s2.setValue(moddedKey, text);
    });

    // ── Native tool worker thread ─────────────────────────────────────────────
    m_pythonThread = new QThread(this);
    m_pyRunner     = new NativeRunner;
    m_pyRunner->moveToThread(m_pythonThread);

    connect(this,        &PipelineTab::runScript,
            m_pyRunner,  &NativeRunner::run);
    connect(m_pyRunner,  &NativeRunner::output,
            m_log,       &LogPanel::appendOutput);
    connect(m_pyRunner,  &NativeRunner::error,
            m_log,       &LogPanel::appendError);
    connect(m_pyRunner,  &NativeRunner::finished,
            this,        &PipelineTab::onScriptFinished);

    m_pythonThread->start();

    // ── extract-xiso worker thread ────────────────────────────────────────────
    m_xisoThread = new QThread(this);
    m_xisoRunner  = new XisoRunner;
    m_xisoRunner->moveToThread(m_xisoThread);

    connect(this,         &PipelineTab::xisoUnpack,
            m_xisoRunner,  &XisoRunner::unpack);
    connect(this,         &PipelineTab::xisoRepack,
            m_xisoRunner,  &XisoRunner::repack);
    connect(m_xisoRunner,  &XisoRunner::output,
            m_log,         &LogPanel::appendOutput);
    connect(m_xisoRunner,  &XisoRunner::error,
            m_log,         &LogPanel::appendError);
    connect(m_xisoRunner,  &XisoRunner::finished,
            this,           &PipelineTab::onScriptFinished);

    m_xisoThread->start();

    // ── ps2isotool worker thread ──────────────────────────────────────────────
    m_ps2Thread = new QThread(this);
    m_ps2Runner  = new Ps2IsoRunner;
    m_ps2Runner->moveToThread(m_ps2Thread);

    connect(this,         &PipelineTab::ps2Extract,
            m_ps2Runner,  &Ps2IsoRunner::extract);
    connect(this,         &PipelineTab::ps2Build,
            m_ps2Runner,  &Ps2IsoRunner::build);
    connect(m_ps2Runner,  &Ps2IsoRunner::output,
            m_log,         &LogPanel::appendOutput);
    connect(m_ps2Runner,  &Ps2IsoRunner::error,
            m_log,         &LogPanel::appendError);
    connect(m_ps2Runner,  &Ps2IsoRunner::finished,
            this,           &PipelineTab::onScriptFinished);

    m_ps2Thread->start();
}

PipelineTab::~PipelineTab()
{
    m_pythonThread->quit();
    m_pythonThread->wait();
    delete m_pyRunner;

    m_xisoThread->quit();
    m_xisoThread->wait();
    delete m_xisoRunner;

    m_ps2Thread->quit();
    m_ps2Thread->wait();
    delete m_ps2Runner;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString PipelineTab::platformStr() const
{
    switch (m_platform) {
    case Platform::GC:   return "GC";
    case Platform::Xbox: return "XBOX";
    case Platform::PS2:  return "PS2";
    }
    return "GC";
}

// Short tag embedded in the session directory name: "GC", "XB", "PS2"
static QString platformTag(Platform p)
{
    switch (p) {
    case Platform::GC:   return "GC";
    case Platform::Xbox: return "XB";
    case Platform::PS2:  return "PS2";
    }
    return "GC";
}


QString PipelineTab::extractedIsoDir() const { return m_workDir + "/" + m_isoName + " extracted iso/"; }
QString PipelineTab::vanillaBecDir()   const { return m_workDir + "/" + m_isoName + " default_BEC/"; }
QString PipelineTab::moddedBecDir()    const { return m_workDir + "/" + m_isoName + " working_BEC/"; }
QString PipelineTab::moddedIsoDir()    const { return m_workDir + "/" + m_isoName + " modded_ISO/"; }
QString PipelineTab::vanillaIsoDir()   const { return extractedIsoDir(); }

QString PipelineTab::resolveSessionDir(const QFileInfo &isoFi, bool createNew) const
{
    QString baseDir  = isoFi.absolutePath();
    QString isoBase  = isoFi.completeBaseName();
    QString tag      = platformTag(m_platform);
    QString date     = QDate::currentDate().toString("dd-MMM-yyyy");
    // Directory format: "<isoName> <GC|XB|PS2> dd-MMM-yyyy"
    QString todayDir = baseDir + "/" + isoBase + " " + tag + " " + date;

    if (createNew) {
        QDir().mkpath(todayDir);
        return todayDir;
    }

    // Pack: prefer today's session dir; otherwise use the most recent existing one
    // Glob includes the platform tag so GC/XB/PS2 sessions never cross-contaminate.
    if (QDir(todayDir).exists())
        return todayDir;

    QFileInfoList candidates = QDir(baseDir).entryInfoList(
        QStringList() << (isoBase + " " + tag + " *"),
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Time);   // newest first

    if (!candidates.isEmpty())
        return candidates.first().absoluteFilePath();

    // Fallback: create today's dir so pack at least has somewhere to work
    QDir().mkpath(todayDir);
    return todayDir;
}

// Returns the full path to a file in dir, matched case-insensitively.
// Falls back to dir + "/" + name if no match is found.
QString PipelineTab::resolveFile(const QString &dir, const QString &name)
{
    const QFileInfoList entries = QDir(dir).entryInfoList(QDir::Files);
    for (const QFileInfo &fi : entries)
        if (fi.fileName().compare(name, Qt::CaseInsensitive) == 0)
            return fi.absoluteFilePath();
    return dir + (dir.endsWith('/') ? "" : "/") + name;
}

// Returns the actual on-disk path of parent/name, matched case-insensitively.
// Ensures a trailing slash. Falls back to parent/name/ if no match found.
QString PipelineTab::resolveSubdir(const QString &parent, const QString &name)
{
    QDir dir(parent);
    const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : entries) {
        if (fi.fileName().compare(name, Qt::CaseInsensitive) == 0)
            return fi.absoluteFilePath() + "/";
    }
    return parent + "/" + name + "/";
}

bool PipelineTab::copyDirExcluding(const QString &src, const QString &dst,
                                    const QString &skipFile) const
{
    QDir srcDir(src);
    if (!srcDir.exists()) return false;
    QDir().mkpath(dst);

    const QFileInfoList entries = srcDir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QFileInfo &fi : entries) {
        if (fi.isDir()) {
            if (!copyDirExcluding(fi.absoluteFilePath(),
                                  dst + "/" + fi.fileName(), skipFile))
                return false;
        } else {
            if (fi.fileName().compare(skipFile, Qt::CaseInsensitive) == 0)
                continue;
            QString destPath = dst + "/" + fi.fileName();
            if (QFile::exists(destPath))
                QFile::remove(destPath);
            if (!QFile::copy(fi.absoluteFilePath(), destPath))
                return false;
        }
    }
    return true;
}

QString PipelineTab::becFileName()    const
{
    // GC and Xbox use gladius.bec; PS2 uses data.bec
    return (m_platform == Platform::PS2) ? "data.bec" : "gladius.bec";
}

QString PipelineTab::moddedIsoPath()  const
{
    QString suffix;
    switch (m_platform) {
    case Platform::GC:   suffix = "GladiusGamecubeModded.iso"; break;
    case Platform::Xbox: suffix = "GladiusXboxModded.iso";     break;
    case Platform::PS2:  suffix = "GladiusPS2Modded.iso";      break;
    }
    return m_workDir + "/" + suffix;
}

// ── UI helpers ────────────────────────────────────────────────────────────────

void PipelineTab::browseIso()
{
    QString path = QFileDialog::getOpenFileName(this,
        "Select vanilla ISO", QString(), "ISO files (*.iso);;All files (*)");
    if (!path.isEmpty())
        m_isoPathEdit->setText(path);
}

void PipelineTab::browseModdedDir()
{
    QString start = m_moddedDirEdit->text().isEmpty()
        ? (m_isoPathEdit->text().isEmpty()
           ? QDir::homePath()
           : QFileInfo(m_isoPathEdit->text().trimmed()).absolutePath())
        : m_moddedDirEdit->text();
    QString dir = QFileDialog::getExistingDirectory(
        this, "Select modded BEC directory (working_BEC)", start);
    if (!dir.isEmpty())
        m_moddedDirEdit->setText(dir);
}

void PipelineTab::setRunning(bool running)
{
    m_unpackBtn->setEnabled(!running);
    m_packBtn->setEnabled(!running);
    m_browseBtn->setEnabled(!running);
    m_moddedDirBrowseBtn->setEnabled(!running);
}

// ── Public action slots ───────────────────────────────────────────────────────

void PipelineTab::unpackVanilla()
{
    QString isoPath = m_isoPathEdit->text().trimmed();
    if (isoPath.isEmpty()) {
        QMessageBox::warning(this, "No ISO selected", "Please select a vanilla ISO file first.");
        return;
    }

    QFileInfo fi(isoPath);
    m_isoName  = fi.completeBaseName(); // strips only last ext
    m_workDir  = resolveSessionDir(fi, /*createNew=*/true);
    m_isUnpack = true;

    m_log->appendOutput("=== Unpack: " + fi.fileName() + " ===");
    setRunning(true);
    m_statusLabel->setText("Running…");

    m_step = Step::UnpackIso;
    startNextStep();
}

void PipelineTab::packModded()
{
    QString overrideDir = m_moddedDirEdit->text().trimmed();

    if (!overrideDir.isEmpty()) {
        if (!QDir(overrideDir).exists()) {
            QMessageBox::warning(this, "Directory not found",
                "The specified modded BEC directory does not exist:\n" + overrideDir);
            return;
        }
        // Derive m_workDir (parent) and m_isoName from the directory name.
        // Expected naming: "<isoName> working_BEC" — strip the suffix if present.
        QDir becDir(overrideDir);
        QString dirName = becDir.dirName();
        static const QString kSuffix = " working_BEC";
        if (dirName.endsWith(kSuffix, Qt::CaseInsensitive))
            m_isoName = dirName.left(dirName.length() - kSuffix.length());
        else
            m_isoName = dirName;
        becDir.cdUp();
        m_workDir = becDir.absolutePath();
    } else {
        // Fall back to ISO-based auto-detection
        QString isoPath = m_isoPathEdit->text().trimmed();
        if (isoPath.isEmpty()) {
            QMessageBox::warning(this, "No directory selected",
                "Either select the modded BEC directory above, or select the vanilla ISO "
                "so the tool can auto-detect the most recent session.");
            return;
        }
        QFileInfo fi(isoPath);
        m_isoName = fi.completeBaseName();
        m_workDir = resolveSessionDir(fi, /*createNew=*/false);
    }

    m_isUnpack = false;

    m_log->appendOutput("=== Pack modded ISO ===");
    setRunning(true);
    m_statusLabel->setText("Running…");

    m_step = Step::TokNumUpdate;
    startNextStep();
}

// ── Pipeline state machine ────────────────────────────────────────────────────

void PipelineTab::runPython(const QString &script, const QStringList &args)
{
    m_log->appendOutput(">> " + script + " " + args.join(' '));
    emit runScript(script, args);
}

void PipelineTab::startNextStep()
{
    switch (m_step) {

    // ── UNPACK STEPS ──────────────────────────────────────────────────────────

    case Step::UnpackIso: {
        QString isoPath = m_isoPathEdit->text().trimmed();
        if (m_platform == Platform::GC) {
            // ngciso-tool -unpack <iso> <outdir/> <filelist>
            runPython("ngciso-tool", {"-unpack", isoPath, vanillaIsoDir(), "GladiusGamecubeVanilla_FileList.txt"});
        } else if (m_platform == Platform::PS2) {
            // ps2isotool extract <iso> <outdir>  — extracts directly into extractedIsoDir
            QString dst = extractedIsoDir();
            if (dst.endsWith('/')) dst.chop(1);
            QDir().mkpath(dst);
            m_log->appendOutput(">> ps2isotool extract " + isoPath + " " + dst);
            emit ps2Extract(isoPath, dst);
        } else {
            // Xbox: extract-xiso creates <isoName>/ relative to its CWD (m_workDir).
            // Remove it if it already exists so re-runs don't fail.
            QString xisoOut = m_workDir + "/" + m_isoName;
            if (QDir(xisoOut).exists())
                QDir(xisoOut).removeRecursively();
            m_log->appendOutput(">> extract-xiso " + isoPath);
            emit xisoUnpack(isoPath, m_workDir);
        }
        break;
    }

    case Step::UnpackBec: {
        // bec-tool --platform <P> -unpack <bec> <becDir/>
        QString becSrc = resolveFile(extractedIsoDir(), becFileName());
        runPython("bec-tool", {"--platform", platformStr(), "-unpack", becSrc, vanillaBecDir()});
        break;
    }

    case Step::UnpackBecModded: {
        // Unpack a second copy into working_BEC — only if it doesn't already exist
        if (QDir(moddedBecDir()).exists()) {
            m_log->appendOutput("(working_BEC already exists — skipping unpack)");
            m_step = Step::UnpackIdx;
            startNextStep();
            return;
        }
        QString becSrc = resolveFile(extractedIsoDir(), becFileName());
        runPython("bec-tool", {"--platform", platformStr(), "-unpack", becSrc, moddedBecDir()});
        break;
    }

    case Step::UnpackIdx: {
        QString dataDir = resolveSubdir(vanillaBecDir(), "data");
        runPython("idx-unpack", {dataDir});
        break;
    }

    case Step::UnpackIdxModded: {
        QString dataDir = resolveSubdir(moddedBecDir(), "data");
        runPython("idx-unpack", {dataDir});
        break;
    }

    // ── PACK STEPS ────────────────────────────────────────────────────────────

    case Step::TokNumUpdate: {
        QString cfg = resolveSubdir(resolveSubdir(moddedBecDir(), "data"), "config");
        if (!QFile::exists(cfg + "skills.tok") || !QFile::exists(cfg + "items.tok")) {
            m_log->appendOutput("(skipping tok-num-update — skills.tok/items.tok not present)");
            m_step = Step::TokCompress;
            startNextStep();
            return;
        }
        runPython("tok-num-update", {cfg});
        break;
    }

    case Step::TokCompress: {
        QString cfg = resolveSubdir(resolveSubdir(moddedBecDir(), "data"), "config");
        if (!QFile::exists(cfg + "skills.tok")) {
            m_log->appendOutput("(skipping tok-tool — skills.tok not present)");
            m_step = Step::UpdateStringsBin;
            startNextStep();
            return;
        }
        // tok-tool -c <skills.tok> <skills_strings.bin> <skills_lines.bin> <skills.tok.brf>
        runPython("tok-tool", {"-c",
            cfg + "skills.tok",
            cfg + "skills_strings.bin",
            cfg + "skills_lines.bin",
            cfg + "skills.tok.brf"
        });
        break;
    }

    case Step::UpdateStringsBin: {
        QString cfg = resolveSubdir(resolveSubdir(moddedBecDir(), "data"), "config");
        if (!QFile::exists(cfg + "lookuptext_eng.txt")) {
            m_log->appendOutput("(skipping update-strings-bin — lookuptext_eng.txt not present)");
            m_step = Step::RepackIdx;
            startNextStep();
            return;
        }
        runPython("update-strings-bin", {cfg});
        break;
    }

    case Step::RepackIdx: {
        QString dataDir = resolveSubdir(moddedBecDir(), "data");
        runPython("idx-repack", {dataDir});
        break;
    }

    case Step::PrepModdedIso: {
        // Copy everything from extracted iso → modded_ISO, skipping gladius.bec
        QString src = extractedIsoDir();
        QString dst = moddedIsoDir();
        dst.chop(1); // remove trailing slash for mkpath
        m_log->appendOutput("Copying base ISO files to modded ISO directory…");
        if (!copyDirExcluding(src, dst, becFileName())) {
            m_log->appendError("Failed to copy base ISO files.");
            m_step = Step::Idle;
            setRunning(false);
            m_statusLabel->setText("Failed.");
            return;
        }
        m_log->appendOutput("Base ISO files copied.");
        m_step = Step::RepackBec;
        startNextStep();
        return;
    }

    case Step::RepackBec: {
        // bec-tool -pack <becDir/> <out.bec> <filelist.txt> --platform <P>
        QString outBec = moddedIsoDir() + becFileName();
        QString fileList = moddedBecDir() + "filelist.txt";
        runPython("bec-tool", {"-pack", moddedBecDir(), outBec, fileList,
                               "--platform", platformStr()});
        break;
    }

    case Step::RepackIso: {
        if (m_platform == Platform::GC) {
            // ngciso-tool -pack <isoDir/> <fst.bin> <fileList> <out.iso>
            QString fst      = moddedIsoDir() + "fst.bin";
            QString fileList = moddedIsoDir() + "GladiusGamecubeModded_FileList.txt";
            runPython("ngciso-tool", {"-pack", moddedIsoDir(), fst, fileList, moddedIsoPath()});
        } else if (m_platform == Platform::PS2) {
            // ps2isotool build <srcDir> <outIso> <volName>
            QString src = moddedIsoDir();
            if (src.endsWith('/')) src.chop(1);
            m_log->appendOutput(">> ps2isotool build " + src + " " + moddedIsoPath());
            emit ps2Build(src, moddedIsoPath(), "GLADIUS");
        } else {
            // Xbox: extract-xiso -c <moddedIsoDir>
            m_log->appendOutput(">> extract-xiso -c " + moddedIsoDir());
            emit xisoRepack(moddedIsoDir(), m_workDir);
        }
        break;
    }

    case Step::Idle:
        break;
    }
}

void PipelineTab::onScriptFinished(bool success)
{
    if (!success) {
        m_log->appendError("Step failed — pipeline aborted.");
        m_step = Step::Idle;
        setRunning(false);
        m_statusLabel->setText("Failed.");
        return;
    }

    // Advance state machine
    switch (m_step) {
    case Step::UnpackIso: {
        // Xbox: extract-xiso creates <isoName>/ in its CWD (m_workDir) — move to "<workDir>/<isoName> extracted iso/"
        // GC/PS2: output already goes directly to extractedIsoDir(), no rename needed.
        if (m_platform == Platform::Xbox) {
            QString src = m_workDir + "/" + m_isoName;
            QString dst = extractedIsoDir();
            dst.chop(1); // remove trailing slash for rename

            if (QDir(dst).exists())
                QDir(dst).removeRecursively();

            if (QDir().rename(src, dst))
                m_log->appendOutput("Extracted ISO to: " + QFileInfo(dst).fileName());
            else
                m_log->appendError("Warning: could not rename extracted folder.");
        }
        m_step = Step::UnpackBec;
        break;
    }
    case Step::UnpackBec:
        m_step = Step::UnpackBecModded; break;
    case Step::UnpackBecModded:
        m_step = Step::UnpackIdx;       break;
    case Step::UnpackIdx:
        m_step = Step::UnpackIdxModded; break;
    case Step::UnpackIdxModded:
        m_step = Step::Idle;
        m_log->appendOutput("=== Unpack complete. Edit files in: " + moddedBecDir() + " ===");
        m_moddedDirEdit->setText(moddedBecDir());
        emit unpackComplete(moddedBecDir(), vanillaBecDir());
        break;

    case Step::TokNumUpdate:
        m_step = Step::TokCompress;   break;
    case Step::TokCompress:
        m_step = Step::UpdateStringsBin; break;
    case Step::UpdateStringsBin:
        m_step = Step::RepackIdx;     break;
    case Step::RepackIdx:
        m_step = Step::PrepModdedIso; break;
    case Step::PrepModdedIso:
        break; // handled inline in startNextStep (no async op)
    case Step::RepackBec:
        m_step = Step::RepackIso;     break;
    case Step::RepackIso:
        m_step = Step::Idle;
        m_log->appendOutput("=== Pack complete. ISO at: " + moddedIsoPath() + " ===");
        break;

    case Step::Idle:
        break;
    }

    if (m_step != Step::Idle) {
        startNextStep();
    } else {
        setRunning(false);
        m_statusLabel->setText("Done.");
    }
}
