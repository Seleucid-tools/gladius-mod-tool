#pragma once
#include <QMainWindow>

class QTabWidget;
class LogPanel;
class PipelineTab;
class EditorTab;
class SchoolBuilderTab;
class QSplitter;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QTabWidget   *m_platformTabs;
    PipelineTab  *m_gcTab;
    PipelineTab  *m_xboxTab;
    PipelineTab  *m_ps2Tab;
    EditorTab         *m_editorTab;
    SchoolBuilderTab  *m_schoolTab;
    LogPanel          *m_logPanel;
    QSplitter    *m_splitter;

    QString       m_lastModdedDir;
    QString       m_lastVanillaDir;

    void buildMenu();
    void restoreSettings();
    void saveSettings();
};
