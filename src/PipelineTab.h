#pragma once
#include <QWidget>
#include <QString>
#include <QThread>

class QLineEdit;
class QPushButton;
class QLabel;
class QFileInfo;
class LogPanel;
class PythonRunner;
class XisoRunner;
class Ps2IsoRunner;

enum class Platform { GC, Xbox, PS2 };

class PipelineTab : public QWidget
{
    Q_OBJECT
public:
    explicit PipelineTab(Platform platform, LogPanel *log, QWidget *parent = nullptr);
    ~PipelineTab();

signals:
    // Forwarded to PythonRunner (lives in worker thread)
    void runScript(const QString &scriptPath, const QStringList &args);

    // Forwarded to XisoRunner (lives in worker thread)
    void xisoUnpack(const QString &isoPath, const QString &workingDir);
    void xisoRepack(const QString &dirPath, const QString &workingDir);

    // Forwarded to Ps2IsoRunner (lives in worker thread)
    void ps2Extract(const QString &isoPath, const QString &outDir);
    void ps2Build  (const QString &srcDir,  const QString &outIso, const QString &volName);

    // Emitted when unpack completes successfully — used to populate editor tab
    // moddedDir  = path to Modded_BEC/   (edit files here)
    // vanillaDir = path to Vanilla_BEC/  (source for revert)
    void unpackComplete(const QString &moddedDir, const QString &vanillaDir);

private slots:
    void browseIso();
    void unpackVanilla();
    void packModded();
    void onScriptFinished(bool success);
    void setRunning(bool running);

private:
    Platform      m_platform;
    LogPanel     *m_log;

    QLineEdit    *m_isoPathEdit;
    QPushButton  *m_browseBtn;
    QPushButton  *m_unpackBtn;
    QPushButton  *m_packBtn;
    QLabel       *m_statusLabel;

    // Worker thread + runners
    QThread      *m_pythonThread;
    PythonRunner *m_pyRunner;
    QThread      *m_xisoThread;
    XisoRunner   *m_xisoRunner;
    QThread      *m_ps2Thread;
    Ps2IsoRunner *m_ps2Runner;

    enum class Step {
        Idle,
        UnpackIso,
        UnpackBec,
        UnpackBecModded,
        UnpackIdx,
        UnpackIdxModded,
        TokNumUpdate,
        TokCompress,
        UpdateStringsBin,
        RepackIdx,
        PrepModdedIso,
        RepackBec,
        RepackIso,
    };

    Step          m_step = Step::Idle;
    QString       m_workDir;
    QString       m_isoName;
    bool          m_isUnpack = false;

    void startNextStep();
    void runPython(const QString &script, const QStringList &args);

    QString scriptPath(const QString &name) const;
    QString platformStr() const;
    QString extractedIsoDir() const;  // "<isoName> extracted iso/"
    QString vanillaBecDir()   const;  // "<isoName> default_BEC/"
    QString moddedBecDir()    const;  // "<isoName> working_BEC/"
    QString moddedIsoDir()    const;
    QString vanillaIsoDir()   const;  // alias for extractedIsoDir (GC)
    QString becFileName()     const;
    QString moddedIsoPath()   const;

    // Creates (unpack) or locates (pack) the dated session directory
    QString resolveSessionDir(const QFileInfo &isoFi, bool createNew) const;

    // Returns the actual on-disk name of a subdirectory, resolving case
    // insensitively. Falls back to the requested name if not found.
    static QString resolveSubdir(const QString &parent, const QString &name);

    // Returns the full path to a file inside dir, matched case-insensitively.
    // Falls back to dir + "/" + name if no match is found.
    static QString resolveFile(const QString &dir, const QString &name);

    // Recursively copies src/ into dst/, skipping any file named skipFile.
    // Returns true on success.
    bool copyDirExcluding(const QString &src, const QString &dst,
                          const QString &skipFile) const;
};
